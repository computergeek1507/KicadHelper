#include "library_finder.h"

#include "kicad_utils.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QFileInfo>
#include <QCoreApplication>

LibraryFinder::LibraryFinder()
{

}
void LibraryFinder::LoadProject(QString const& folder)
{
    libraryList.clear();
    m_projectFolder = folder;
    getProjectLibraries();
    getStockLibraries();
}

void LibraryFinder::getProjectLibraries()
{
    QString localLibPath = m_projectFolder + "/fp-lib-table";
    if(QFile::exists(localLibPath))
    {
        ParseLibraries(localLibPath, "Project");
    }
}

void LibraryFinder::getStockLibraries()
{
    if(QFile::exists(getGlobalFootprintTablePath()))
    {
        ParseLibraries(getGlobalFootprintTablePath(), "System");
    }
    else
    {
        emit SendMessage(QString("Could not Open '%1'").arg(getGlobalFootprintTablePath()), spdlog::level::level_enum::warn);
    }
}

void LibraryFinder::ParseLibraries(QString const& path, QString const& level)
{
	QFile inFile(path);

	if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		emit SendMessage(QString("Could not Open '%1'").arg(path), spdlog::level::level_enum::warn);
		return;
	}
	QTextStream in(&inFile);
	while (!in.atEnd())
	{
		QString line = in.readLine();
        QString name = getLibParamter("name", line);
        QString type = getLibParamter("type", line);
        QString uri = getLibParamter("uri", line);
        QString options = getLibParamter("options", line);
        QString descr = getLibParamter("descr", line);
        if(!name.isEmpty() && !type.isEmpty() && !uri.isEmpty() )
        {
            emit AddLibrary(level, name, type , uri);
            libraryList.emplace_back(name, type , uri, options, descr, level);
        }
	}
	inFile.close();
}

void LibraryFinder::SaveLibraryTable(QString const& fileName)
{
    QFile outFile(fileName);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        emit SendMessage(QString("Could not Open '%1'").arg(fileName), spdlog::level::level_enum::warn);
        return;
    }
    QTextStream out(&outFile);

    out << "(fp_lib_table\n";
    for (auto const& library : libraryList)
    {
        if (library.level != "Project")
        {
            continue;
        }
        out << library.asString() << "\n";
    }
    out << ")\n";
    outFile.close();
    emit SendMessage(QString("Saved Library Table to '%1'").arg(outFile.fileName()), spdlog::level::level_enum::debug);
}

bool LibraryFinder::CheckSchematics()
{
	if (libraryList.empty())
	{
		emit SendMessage("Library List is empty", spdlog::level::level_enum::warn);
		return false;
	}

    CreateFootprintList();

	QDir directory(m_projectFolder);
	if (!directory.exists())
	{
		emit SendMessage("Directory Doesn't Exist", spdlog::level::level_enum::warn);
		return false;
	}

    missingfootprintList.clear();

    auto const& kicadFiles {directory.entryInfoList(QStringList() << "*.kicad_sch" , QDir::Files)};
	for (auto const& file : kicadFiles)
	{
		emit SendMessage(QString("Checking '%1'").arg(file.fileName()), spdlog::level::level_enum::debug);
		CheckSchematic(file.absoluteFilePath());
	}
	return true;
}

void LibraryFinder::CheckSchematic(QString const& schPath)
{
    QFileInfo fileData(schPath);
    QFile inFile(schPath);

	if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		emit SendMessage(QString("Could not Open '%1'").arg(schPath), spdlog::level::level_enum::warn);
		return;
	}
    bool errorFound{false};
    bool partSection{false};
    //int lineCount{0};

    QString reference;
	QTextStream in(&inFile);
	while (!in.atEnd())
	{
        //lineCount++;
		QString line = in.readLine();

        if(line.contains("(symbol (lib_id"))
		{
			partSection = true;
		}
        if(!partSection)
		{
			continue;
		}

        QString ref = getSchReference( line);
        if(!ref.isEmpty())
        {
            reference = ref;
        } 

        QString footprint = getSchFootprint( line);
        if(footprint.isEmpty())
        {
            continue;
        } 
        if(!HasFootPrint(footprint))
        {
            if (missingfootprintList.contains(footprint))
            {
                missingfootprintList.append(footprint);
            }
            emit SendResult(QString("'%1':'%2' was not found in '%3'").arg(reference).arg(footprint).arg(fileData.fileName()),true);
            errorFound = true;
        }
        reference.clear();
	}
	inFile.close();

    if(!errorFound)
    {
        emit SendResult(QString("'%1' is Good").arg(fileData.fileName()), false);
    }
}

bool LibraryFinder::HasFootPrint(QString const& footprint) const
{
    if(!footprint.contains(":"))
    {
        return false;
    }
    auto parts {footprint.split(":")};

    if(!footprintList.contains(parts[0]))
    {
        return false;
    }

    auto const& list = footprintList.value(parts[0]);

    return list.contains(parts[1]);
}

void LibraryFinder::CreateFootprintList()
{
    footprintList.clear();

    for(auto const& lib : libraryList)
    {
        if(footprintList.contains(lib.name))
        {
            SendMessage(QString("Duplicate Libraries %1").arg(lib.name), spdlog::level::level_enum::warn);
            continue;
        }
        QStringList fps; 
        if(lib.type == "Legacy")
        {
            fps = GetLegacyFootPrints(lib.url);
        }
        else if(lib.type == "KiCad")
        {
            fps = GetKicadFootPrints(lib.url);
        }

        if(!fps.empty())
        {
            footprintList.insert(lib.name,fps );
        }
    }
}

QStringList LibraryFinder::GetLegacyFootPrints(QString const& url) const
{
    QStringList list;

    auto fullPath = updatePath(url);

    //QFileInfo fileData(fullPath);
    QFile inFile(fullPath);

    if(!inFile.exists())
    {
        return list;
    }

	if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		return list;
	}

	QTextStream in(&inFile);
	while (!in.atEnd())
	{
		QString line = in.readLine();

        if(!line.startsWith("DEF"))
		{
			continue;
		}

        auto parts = line.split(" ", Qt::SkipEmptyParts);

        if(parts.count() < 2)
        {
            continue;
        }

        auto name{parts[1]};

        name = kicad_utils::CleanQuotes(name);

        if(name.isEmpty())
        {
            continue;
        }

        list.append(name);
	}
	inFile.close();


    return list;
}

QStringList LibraryFinder::GetKicadFootPrints(QString const& url) const
{
    QStringList list;

    auto fullPath = updatePath(url);

    QDir directory(fullPath);
    if(!directory.exists())
    {
        return list;
    }

    auto const& kicadFiles = directory.entryInfoList(QStringList() << "*.kicad_mod" , QDir::Files );
	for (auto const& file : kicadFiles)
	{
        list.append(file.completeBaseName());
    }

    return list;
}

QString LibraryFinder::updatePath(QString path) const
{
    path = path.replace("${KICAD6_FOOTPRINT_DIR}", getGlobalFootprintsPath() );
    path = path.replace("${KIPRJMOD}", m_projectFolder);
    return path;
}

QString LibraryFinder::getGlobalFootprintsPath() const
{
    //C:\Program Files\KiCad\6.0\share\kicad\footprints

#if defined( Q_OS_DARWIN )
    return R"(kicad.app/Contents/SharedSupport/footprints)";
#elif defined( Q_OS_WIN )
#ifdef _DEBUG
    return R"(C:/Program Files/KiCad/6.0/share/kicad/footprints/)";
#else
    QFileInfo root( QCoreApplication::applicationDirPath() +  ( "../share/kicad/footprints/" ) );
    return root.absoluteFilePath();
#endif 
#else
    return R"(/usr/share/kicad/footprints/)" ;
#endif
}

QString LibraryFinder::getGlobalFootprintTablePath() const
{
#if defined( Q_OS_DARWIN )
    return R"(kicad.app/Contents/SharedSupport/template/fp-lib-table)";
#elif defined( Q_OS_WIN )
#ifdef _DEBUG
    return R"(C:/Program Files/KiCad/6.0/share/kicad/template/fp-lib-table)";
#else
    QFileInfo root( QCoreApplication::applicationDirPath() +  ( "../share/kicad/template/fp-lib-table" ) );
    return root.absoluteFilePath();
#endif 
#else
    return R"(/usr/share/kicad/template/fp-lib-table)" ;
#endif
    //C:\Program Files\KiCad\6.0\share\kicad\template\fp-lib-table
}

QString LibraryFinder::getLibParamter(QString const& parm, QString const& line ) const
{
    int stInd =  line.indexOf("(" + parm );
    int endInd =  line.indexOf(")",stInd );
   
    if(stInd == endInd || stInd ==-1 || endInd == -1)
    {
        return QString();
    }
    int nstr {stInd + parm.length() + 2};
    QString value = line.mid(nstr, endInd - nstr);
   
    value = kicad_utils::CleanQuotes(value);
    return value;
}

QString LibraryFinder::getSchFootprint(QString const& line ) const
{
    QString prop(R"((property "Footprint")");
    int stInd =  line.indexOf(prop );

    if(stInd == -1 )
    {
         return QString();
    }
    int endInd =  line.indexOf("(",stInd + 1 );

    if(stInd == endInd || stInd ==-1 || endInd == -1)
    {
        return QString();
    }
    int nstr {stInd + prop.size() + 1};
    QString value = line.mid(nstr, endInd - nstr - 1);
  
    value = kicad_utils::CleanQuotes(value);
    return value;
}

QString LibraryFinder::getSchReference(QString const& line ) const
{
    QString prop(R"((property "Reference")");
    int stInd =  line.indexOf(prop );

    if(stInd == -1 )
    {
         return QString();
    }
    int endInd =  line.indexOf("(",stInd + 1 );

    if(stInd == endInd || stInd ==-1 || endInd == -1)
    {
        return QString();
    }
    int nstr {stInd + prop.size() + 1};
    QString value = line.mid(nstr, endInd - nstr - 1);
  
    value = kicad_utils::CleanQuotes(value);
    return value;
}

bool LibraryFinder::FixFootPrints(QString const& folder) 
{
    if (missingfootprintList.empty())
    {
        CheckSchematics();
    }
    for (auto const& footprint : missingfootprintList)
    {

    }
    return true;
}

void LibraryFinder::ConvertToRelativePath(QString const& path, QString const& folder)
{
    if (path.startsWith(folder))
    {
        
    }
}