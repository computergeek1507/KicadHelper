#include "library_finder.h"

#include "kicad_utils.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QFileInfo>
#include <QCoreApplication>

constexpr std::string_view PROJ_FOLDER = "${KIPRJMOD}";

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
        ParseLibraries(localLibPath, PROJECT_LIB);
    }
}

void LibraryFinder::getStockLibraries()
{
    if(QFile::exists(getGlobalFootprintTablePath()))
    {
        ParseLibraries(getGlobalFootprintTablePath(), SYSTEM_LIB);
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
            libraryList[level].emplace_back(name, type, uri, options, descr);
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
    for (auto const& library : libraryList[PROJECT_LIB])
    {
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
            if (!missingfootprintList.contains(footprint))
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

    for(auto const& library : libraryList)
    {
        for(auto const& lib : library)
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
                footprintList.insert(lib.name, fps);
            }
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
    path = path.replace(PROJ_FOLDER.data(), m_projectFolder);
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
    bool worked{false};
    /*
        Three Steps of "fixing" footprints
        1) Convert absolute paths to relative paths in Footprint Table
        2) Find librarys in library folder and add Footprint Table
        3) IDK three sounded good 
    */
    if (missingfootprintList.empty())
    {
        CheckSchematics();
    }

    if(ConvertAllPathsToRelative(folder))
    {
        QString localLibPath {m_projectFolder + "/fp-lib-table"};
        SaveLibraryTable(localLibPath);

        emit ClearLibrary(PROJECT_LIB);
        libraryList[PROJECT_LIB].clear();
        getProjectLibraries();
        emit ClearResults();
        CheckSchematics();
    }

    for (auto const& footprint : missingfootprintList)
    {
        if(AttemptToFindFootPrintPath(footprint,folder))
        {
            worked = true;
        }
    }
    if(worked)
    {
        QString localLibPath {m_projectFolder + "/fp-lib-table"};
        SaveLibraryTable(localLibPath);

        emit ClearLibrary(PROJECT_LIB);
        libraryList[PROJECT_LIB].clear();
        getProjectLibraries();
        emit ClearResults();
        CheckSchematics();
    }

    return worked;
}

bool LibraryFinder::AttemptToFindFootPrintPath(QString const& footprint, QString const& libraryPath )
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
            auto Footprints = GetLegacyFootPrints(KicdLibPath);
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

bool LibraryFinder::ConvertAllPathsToRelative(QString const& libraryPath)
{
    bool worked{false};
    for (auto & path : libraryList[PROJECT_LIB])
    {
        auto newPath {ConvertToRelativePath(path.url, libraryPath)};
        if(newPath.compare(path.url) != 0)
        {
            path.url = newPath;
            worked = true;
        }
    }
    return worked;
}

QString LibraryFinder::ConvertToRelativePath(QString const& ogpath, QString const& libraryPath)
{
    //if already using macros, avoid
    if(ogpath.startsWith("$"))
    {
        return ogpath;
    }


    auto path{ogpath};
     //if its in a subfolder, easy to do find replace
    if(path.startsWith(m_projectFolder))
    {
        path = path.replace(m_projectFolder, PROJ_FOLDER.data());
        emit SendMessage(QString("Converted '%1' to '%2'").arg( ogpath ).arg(path), spdlog::level::level_enum::debug);
        return path;
    }

    auto startProject{m_projectFolder};
    auto startLibrary{libraryPath};

    QString startLibRelative{QString(PROJ_FOLDER.data()) + "/"};//list of slashes going up the folder structure
    QString folders;                        //list of folders going down the folder structure

    bool worked{false};
    for(int i = 0; i < 255; ++i)//avoid infinite loops
    {
        int netProjSlash = startProject.lastIndexOf("/");
        int netLibSlash = startLibrary.lastIndexOf("/");
        if(-1 == netProjSlash || -1 == netLibSlash)
        {
            break;
        }

        QString const folder = startLibrary.right(startLibrary.size() - (netLibSlash + 1));
        startLibRelative += "../";
        folders += folder;
        folders += "/";

        //get next directory up the folder structure
        startProject = startProject.left(netProjSlash);
        startLibrary = startLibrary.left(netLibSlash);

        //if commom, we are done
        if(startLibrary.startsWith(startProject))
        {
            worked = true;
            break;
        }
    }
    if (worked)
    {
        QString newFolder = startLibRelative + folders;//added up and downs paths together
        path = path.replace(libraryPath + "/", newFolder);
        emit SendMessage(QString("Converted '%1' to '%2'").arg( ogpath ).arg(path), spdlog::level::level_enum::debug);
        return path;
    }
    return path;
}

void LibraryFinder::AddLibraryPath(QString name, QString type, QString url, QString const& level)
{
    if (auto const found { 
            std::find_if(libraryList[level].begin(), libraryList[level].end(), [&](auto const & elem)
		    { return elem.name == name; })
        };
    found == libraryList[level].end()) 
	{
		libraryList[level].emplace_back(name, type,url, "","" );
        emit SendMessage(QString("Adding '%1'").arg( name ), spdlog::level::level_enum::debug);
	}
	else
	{
		auto index = std::distance(libraryList[level].begin(), found);
		libraryList[level][index] = std::move(LibraryInfo(name, type, url, "","" ));
        emit SendMessage(QString("Updating '%1'").arg( name ), spdlog::level::level_enum::debug);
	}
}

QString LibraryFinder::FindRecurseDirectory(const QString& startDir, const QString& dirName) const
{
	QDir dir(startDir);
	QFileInfoList list = dir.entryInfoList();
	for (int iList=0;iList<list.count();iList++)
	{
		QFileInfo info = list[iList];
 
		QString sFilePath = info.filePath();
		if (info.isDir())
		{
            if( info.completeBaseName().compare(dirName) == 0)
            {
                return info.filePath();
            }
			// recursive
			if (info.fileName()!=".." && info.fileName()!=".")
			{
                auto const& path{FindRecurseDirectory(sFilePath, dirName)};
                if(!path.isEmpty())
                {
                    return path;
                }
			}
		}
	}
    return QString();
}

QString LibraryFinder::FindRecurseFile(const QString& startDir, const QStringList& fileNames) const
{
	QDir dir(startDir);
	dir.setNameFilters(fileNames);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);

    auto const& fileList { dir.entryInfoList()};
    if(fileList.count()>0)
    {
        return fileList[0].filePath();
    }

    dir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    QStringList dirList = dir.entryList();
    for (int i=0; i<dirList.size(); ++i)
    {
        QString newPath = QString("%1/%2").arg(dir.absolutePath()).arg(dirList.at(i));
        QString path{FindRecurseFile(newPath, fileNames)};
        if(!path.isEmpty())
        {
            return path;
        }
    }
    return QString();
}