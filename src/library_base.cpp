#include "library_base.h"

#include "kicad_utils.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QCoreApplication>

constexpr std::string_view PROJ_FOLDER = "${KIPRJMOD}";

void LibraryBase::LoadProject(QString const& folder)
{
    libraryList.clear();
    m_projectFolder = folder;
    getProjectLibraries();
    getGlobalLibraries();
}

void LibraryBase::getProjectLibraries()
{
    QString localLibPath{ getProjectLibraryPath() };
    if (QFile::exists(localLibPath))
    {
        ParseLibraries(localLibPath, PROJECT_LIB);
    }
}

void LibraryBase::getGlobalLibraries()
{
    if (QFile::exists(getGlobalLibraryPath()))
    {
        ParseLibraries(getGlobalLibraryPath(), GLOBAL_LIB);
    }
    else
    {
        emit SendMessage(QString("Could not Open '%1'").arg(getGlobalLibraryPath()), spdlog::level::level_enum::warn, getGlobalLibraryPath());
    }
}

QString LibraryBase::getGlobalKicadDataPath() const
{
#if defined( Q_OS_DARWIN )
    return R"(kicad.app/Contents/SharedSupport)";
#elif defined( Q_OS_WIN )
    //#ifdef _DEBUG
    return R"(C:/Program Files/KiCad/6.0/share/kicad)";
    //#else
    //    QFileInfo root( QCoreApplication::applicationDirPath() +  ( "../share/kicad" ) );
    //    return root.absoluteFilePath();
    //#endif
#else
    return R"(/usr/share/kicad)";
#endif    
}

QString LibraryBase::getGlobalFootprintPath() const
{
    //C:\Program Files\KiCad\6.0\share\kicad\footprints
    return getGlobalKicadDataPath() + "/footprints";
}

QString LibraryBase::getGlobalSymbolPath() const 
{
    return getGlobalKicadDataPath() + "/symbols";
}

QString LibraryBase::getGlobalTemplatePath() const
{
    //C:\Program Files\KiCad\6.0\share\kicad\template
    return getGlobalKicadDataPath() + "/template";
}

QString LibraryBase::getGlobalFootprintTablePath() const
{
    return getGlobalTemplatePath() + "/fp-lib-table";
}

QString LibraryBase::getGlobalSymbolTablePath() const
{
    return getGlobalTemplatePath() + "/sym-lib-table";
}

QString LibraryBase::updatePath(QString path) const
{
    path = path.replace("${KICAD6_FOOTPRINT_DIR}", getGlobalFootprintPath() );
    path = path.replace("${KICAD6_SYMBOL_DIR}", getGlobalSymbolPath());
    path = path.replace(PROJ_FOLDER.data(), m_projectFolder);
    return path;
}

bool LibraryBase::ConvertAllPathsToRelative(QString const& libraryPath)
{
    bool worked{ false };
    for (auto& path : libraryList[PROJECT_LIB])
    {
        auto newPath{ ConvertToRelativePath(path.url, libraryPath) };
        if (newPath.compare(path.url) != 0)
        {
            path.url = newPath;
            worked = true;
        }
    }
    return worked;
}

void LibraryBase::AddLibraryPath(QString name, QString type, QString url, QString const& level)
{
    if (auto const found{
            std::find_if(libraryList[level].begin(), libraryList[level].end(), [&](auto const& elem)
            { return elem.name == name; })
        };
        found == libraryList[level].end())
    {
        libraryList[level].emplace_back(name, type, url, "", "");
        emit SendMessage(QString("Adding '%1'").arg(name), spdlog::level::level_enum::debug, QString());
    }
    else
    {
        auto index = std::distance(libraryList[level].begin(), found);
        libraryList[level][index] = std::move(LibraryInfo(name, type, url, "", ""));
        emit SendMessage(QString("Updating '%1'").arg(name), spdlog::level::level_enum::debug, QString());
    }
}

void LibraryBase::ChangeLibraryName(QString const& oldName, QString const& newName, int row)
{
    if (auto const found{
            std::find_if(libraryList[PROJECT_LIB].begin(), libraryList[PROJECT_LIB].end(), [&](auto const& elem)
            { return elem.name == oldName; })
        };
        found != libraryList[PROJECT_LIB].end())
    {
        auto index = std::distance(libraryList[PROJECT_LIB].begin(), found);
        libraryList[PROJECT_LIB][index].name = newName;
        emit SendUpdateLibraryRow(PROJECT_LIB, libraryList[PROJECT_LIB][index].name,
            libraryList[PROJECT_LIB][index].type, libraryList[PROJECT_LIB][index].descr,
            libraryList[PROJECT_LIB][index].url, row);
        SaveLibraryTable(getProjectLibraryPath());
    }
}

void LibraryBase::ChangeLibraryPath(QString const& name, QString const& newPath, int row)
{
    if (auto const found{
            std::find_if(libraryList[PROJECT_LIB].begin(), libraryList[PROJECT_LIB].end(), [&](auto const& elem)
            { return elem.name == name; })
        };
        found != libraryList[PROJECT_LIB].end())
    {
        auto index = std::distance(libraryList[PROJECT_LIB].begin(), found);
        libraryList[PROJECT_LIB][index].url = newPath;
        emit SendUpdateLibraryRow(PROJECT_LIB, libraryList[PROJECT_LIB][index].name,
            libraryList[PROJECT_LIB][index].type, libraryList[PROJECT_LIB][index].descr,
            libraryList[PROJECT_LIB][index].url, row);
        SaveLibraryTable(getProjectLibraryPath());
    }
}

void LibraryBase::ChangeLibraryDescr(QString const& name, QString const& newDescr, int row)
{
    if (auto const found{
            std::find_if(libraryList[PROJECT_LIB].begin(), libraryList[PROJECT_LIB].end(), [&](auto const& elem)
            { return elem.name == name; })
        };
        found != libraryList[PROJECT_LIB].end())
    {
        auto index = std::distance(libraryList[PROJECT_LIB].begin(), found);
        libraryList[PROJECT_LIB][index].descr = newDescr;
        emit SendUpdateLibraryRow(PROJECT_LIB, libraryList[PROJECT_LIB][index].name,
            libraryList[PROJECT_LIB][index].type, libraryList[PROJECT_LIB][index].descr,
            libraryList[PROJECT_LIB][index].url, row);
        SaveLibraryTable(getProjectLibraryPath());
    }
}

void LibraryBase::ChangeLibraryType(QString const& name, QString const& newType, int row)
{
    if (auto const found{
            std::find_if(libraryList[PROJECT_LIB].begin(), libraryList[PROJECT_LIB].end(), [&](auto const& elem)
            { return elem.name == name; })
        };
        found != libraryList[PROJECT_LIB].end())
    {
        auto index = std::distance(libraryList[PROJECT_LIB].begin(), found);
        libraryList[PROJECT_LIB][index].type = newType;
        emit SendUpdateLibraryRow(PROJECT_LIB, libraryList[PROJECT_LIB][index].name,
            libraryList[PROJECT_LIB][index].type, libraryList[PROJECT_LIB][index].descr,
            libraryList[PROJECT_LIB][index].url, row);
        SaveLibraryTable(getProjectLibraryPath());
    }
}

void LibraryBase::RemoveLibrary(QString const& name)
{
     if (auto const found{
            std::find_if(libraryList[PROJECT_LIB].begin(), libraryList[PROJECT_LIB].end(), [&](auto const& elem)
            { return elem.name == name; })
        };
        found != libraryList[PROJECT_LIB].end())
    {
        libraryList[PROJECT_LIB].erase(found);

        SaveLibraryTable(getProjectLibraryPath());
        emit SendClearLibrary(PROJECT_LIB);
        libraryList[PROJECT_LIB].clear();
        getProjectLibraries();
    }
}

void LibraryBase::ImportLibrary(QString const& path, QString const& libFolder)
{
    auto lib {DecodeLibraryInfo(path, libFolder)};

    AddLibraryPath( lib.name, lib.type, lib.url, PROJECT_LIB);

    SaveLibraryTable(getProjectLibraryPath());
    emit SendClearLibrary(PROJECT_LIB);
    libraryList[PROJECT_LIB].clear();
    getProjectLibraries();
}

void LibraryBase::ParseLibraries(QString const& path, QString const& level)
{
    QFile inFile(path);

    if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        emit SendMessage(QString("Could not Open '%1'").arg(path), spdlog::level::level_enum::warn, path);
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
        if (!name.isEmpty() && !type.isEmpty() && !uri.isEmpty())
        {
            emit SendAddLibrary(level, name, type, descr, uri);
            libraryList[level].emplace_back(name, type, uri, options, descr);
        }
    }
    inFile.close();
}

QString LibraryBase::getSchFootprint(QString const& line ) const
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

QString LibraryBase::getSchReference(QString const& line ) const
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

QString LibraryBase::getSchSymbol(QString const& line ) const
{
    QString prop(R"((symbol (lib_id )");
    int stInd =  line.indexOf(prop );

    if(stInd == -1 )
    {
         return QString();
    }
    int endInd =  line.indexOf(")",stInd + 1 );

    if(stInd == endInd || stInd ==-1 || endInd == -1)
    {
        return QString();
    }
    int nstr {stInd + prop.size() + 1};
    QString value = line.mid(nstr, endInd - nstr - 1);

    value = kicad_utils::CleanQuotes(value);
    return value;
}

QString LibraryBase::getLibSymbol(QString const& line ) const
{
    QString prop(R"((symbol )");
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

QString LibraryBase::getLibParamter(QString const& parm, QString const& line ) const
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

QString LibraryBase::ConvertToRelativePath(QString const& ogpath, QString const& libraryPath) const
{
    //if already using macros, avoid
    if(ogpath.startsWith("$"))
    {
        return ogpath;
    }

    if(libraryPath.isEmpty())
    {
        return ogpath;
    }

    auto path{ogpath};
     //if its in a subfolder, easy to do find replace
    if(path.startsWith(m_projectFolder))
    {
        path = path.replace(m_projectFolder, PROJ_FOLDER.data());
        emit SendMessage(QString("Converted '%1' to '%2'").arg( ogpath ).arg(path), spdlog::level::level_enum::debug, QString());
        return path;
    }

    auto startProject{m_projectFolder};
    auto startLibrary{libraryPath};

    QString startLibRelative{QString(PROJ_FOLDER.data()) + "/"};//list of slashes going up the folder structure
    QString folders;                        //list of folders  going down the folder structure

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
        emit SendMessage(QString("Converted '%1' to '%2'").arg( ogpath ).arg(path), spdlog::level::level_enum::debug, QString());
        return path;
    }
    return path;
}

QString LibraryBase::FindRecurseDirectory(const QString& startDir, const QString& dirName) const
{
	QDir dir(startDir);
	QFileInfoList list = dir.entryInfoList();
	for (int iList = 0;iList<list.count();iList++)
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

QString LibraryBase::FindRecurseFile(const QString& startDir, const QStringList& fileNames) const
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

QStringList LibraryBase::FindRecurseFiles(const QString& startDir, const QStringList& fileNames) const
{
    QStringList returnList;
	QDir dir(startDir);
	dir.setNameFilters(fileNames);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);

    for(auto const& file : dir.entryInfoList())
    {
         returnList.append(file.filePath());
    }

    dir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    QStringList dirList = dir.entryList();
    for (int i=0; i<dirList.size(); ++i)
    {
        QString newPath = QString("%1/%2").arg(dir.absolutePath()).arg(dirList.at(i));
        auto paths{FindRecurseFiles(newPath, fileNames)};
        if(!paths.isEmpty())
        {
            returnList.append(paths);
        }
    }
    return returnList;
}