#include "symbol_finder.h"

#include "kicad_utils.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QFileInfo>
#include <QCoreApplication>


SymbolFinder::SymbolFinder()
{

}

void SymbolFinder::getProjectLibraries()
{
    QString localLibPath = m_projectFolder + "/sym-lib-table";
    if(QFile::exists(localLibPath))
    {
        ParseLibraries(localLibPath, PROJECT_LIB);
    }
}

void SymbolFinder::getGlobalLibraries()
{
    if(QFile::exists(getGlobalFootprintTablePath()))
    {
        ParseLibraries(getGlobalFootprintTablePath(), GLOBAL_LIB);
    }
    else
    {
        emit SendMessage(QString("Could not Open '%1'").arg(getGlobalFootprintTablePath()), spdlog::level::level_enum::warn);
    }
}

void SymbolFinder::SaveLibraryTable(QString const& fileName)
{
    QFile outFile(fileName);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        emit SendMessage(QString("Could not Open '%1'").arg(fileName), spdlog::level::level_enum::warn);
        return;
    }
    QTextStream out(&outFile);

    out << "(fp_lib_table\n";
    for (auto const& library : libraryList[PROJECT_LIB])
    {
        out << library.asString() << "\n";
    }
    out << ")\n";
    outFile.close();
    emit SendMessage(QString("Saved Library Table to '%1'").arg(outFile.fileName()), spdlog::level::level_enum::debug);
}

bool SymbolFinder::CheckSchematics()
{
	if (libraryList.empty())
	{
		emit SendMessage("Library List is empty", spdlog::level::level_enum::warn);
		return false;
	}

    CreateSymbolList();

	QDir directory(m_projectFolder);
	if (!directory.exists())
	{
		emit SendMessage("Directory Doesn't Exist", spdlog::level::level_enum::warn);
		return false;
	}

    missingSymbolList.clear();

    auto const& kicadFiles {directory.entryInfoList(QStringList() << "*.kicad_sch" , QDir::Files)};
	for (auto const& file : kicadFiles)
	{
		emit SendMessage(QString("Checking '%1'").arg(file.fileName()), spdlog::level::level_enum::debug);
		CheckSchematic(file.absoluteFilePath());
	}
	return true;
}

void SymbolFinder::CheckSchematic(QString const& schPath)
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
        if(!HasSymbol(footprint))
        {
            if (!missingSymbolList.contains(footprint))
            {
                missingSymbolList.append(footprint);
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

bool SymbolFinder::HasSymbol(QString const& footprint) const
{
    if(!footprint.contains(":"))
    {
        return false;
    }
    auto parts {footprint.split(":")};

    if(!SymbolList.contains(parts[0]))
    {
        return false;
    }

    auto const& list = SymbolList.value(parts[0]);

    return list.contains(parts[1]);
}

void SymbolFinder::CreateSymbolList()
{
    SymbolList.clear();

    for(auto const& library : libraryList)
    {
        for(auto const& lib : library)
        {
            if(SymbolList.contains(lib.name))
            {
                SendMessage(QString("Duplicate Libraries %1").arg(lib.name), spdlog::level::level_enum::warn);
                continue;
            }
            QStringList fps;
            if(lib.type == "Legacy")
            {
                fps = GetLegacySymbols(lib.url);
            }
            else if(lib.type == "KiCad")
            {
                fps = GetKicadSymbols(lib.url);
            }

            if(!fps.empty())
            {
                SymbolList.insert(lib.name, fps);
            }
        }
    }
}

QStringList SymbolFinder::GetLegacySymbols(QString const& url) const
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

        if(!line.startsWith("Li"))
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

QStringList SymbolFinder::GetKicadSymbols(QString const& url) const
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

QString SymbolFinder::getSchFootprint(QString const& line ) const
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

QString SymbolFinder::getSchReference(QString const& line ) const
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

bool SymbolFinder::FixSymbols(QString const& folder)
{
    bool worked{false};
    /*
        Three Steps of "fixing" footprints
        1) Convert absolute paths to relative paths in Footprint Table
        2) Find librarys in library folder and add Footprint Table
        3) IDK 
    */
    if (missingSymbolList.empty())
    {
        CheckSchematics();
    }

    if(ConvertAllPathsToRelative(folder))
    {
        QString localLibPath {m_projectFolder + "/sym-lib-table"};
        SaveLibraryTable(localLibPath);

        emit ClearLibrary(PROJECT_LIB);
        libraryList[PROJECT_LIB].clear();
        getProjectLibraries();
        emit ClearResults();
        CheckSchematics();
    }

    for (auto const& footprint : missingSymbolList)
    {
        if(AttemptToFindSymbolPath(footprint,folder))
        {
            worked = true;
        }
    }
    if(worked)
    {
        QString localLibPath {m_projectFolder + "/sym-lib-table"};
        SaveLibraryTable(localLibPath);

        emit ClearLibrary(PROJECT_LIB);
        libraryList[PROJECT_LIB].clear();
        getProjectLibraries();
        emit ClearResults();
        CheckSchematics();
    }

    return worked;
}

bool SymbolFinder::AttemptToFindSymbolPath(QString const& footprint, QString const& libraryPath )
{
    if(!footprint.contains(":"))
    {
        return false;
    }
    auto parts {footprint.split(":")};

    QString const Kicd6FootPrintName{parts[1]+ ".kicad_mod"};

    QString KicdLibPath{FindRecurseFile(libraryPath, QStringList(Kicd6FootPrintName))};

    if(!KicdLibPath.isEmpty())
    {
        //convert from footprint file path to library folder path
        //convert to relative path
        //add to library file
        QFileInfo prettyPath(KicdLibPath);
        auto newPath{ConvertToRelativePath(prettyPath.absolutePath(),libraryPath )};

        emit SendMessage(QString("Found '%1' footprint in '%2'").arg(footprint).arg(newPath), spdlog::level::level_enum::debug);
        AddLibraryPath(parts[0],"KiCad", newPath, PROJECT_LIB);
        return true;
    }
    else
    {
        QStringList const KicdLegacyLibName{parts[0]+ ".mod", parts[1]+ ".mod", parts[0]+ "*.mod", parts[1]+ "*.mod"};
        KicdLibPath = FindRecurseFile(libraryPath, KicdLegacyLibName);
        if(!KicdLibPath.isEmpty())
        {
            //legacy
            auto Footprints = GetLegacySymbols(KicdLibPath);
            if(Footprints.contains(parts[1]))
            {
                //found something
                //convert to relative path
                //add to library file
                auto newPath{ConvertToRelativePath(KicdLibPath,libraryPath)};
                emit SendMessage(QString("Found '%1' footprint in '%2'").arg(footprint).arg(newPath), spdlog::level::level_enum::debug);
                AddLibraryPath(parts[0],"Legacy", newPath,PROJECT_LIB);
                return true;
            }
        }
    }
    return false;
}


