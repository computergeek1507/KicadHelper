#include "footprint_finder.h"

#include "kicad_utils.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QFileInfo>
#include <QCoreApplication>

FootprintFinder::FootprintFinder()
{

}

QString FootprintFinder::getProjectLibraryPath() const
{
    return m_projectFolder + "/fp-lib-table";
}

QString FootprintFinder::getGlobalLibraryPath() const
{
    return getGlobalFootprintTablePath();
}

void FootprintFinder::SaveLibraryTable(QString const& fileName)
{
    QFile outFile(fileName);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        emit SendMessage(QString("Could not Open '%1'").arg(fileName), spdlog::level::level_enum::warn, fileName);
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
    emit SendMessage(QString("Saved Library Table to '%1'").arg(outFile.fileName()), spdlog::level::level_enum::debug, outFile.fileName());
}

bool FootprintFinder::CheckSchematics()
{
	if (libraryList.empty())
	{
		emit SendMessage("Library List is empty", spdlog::level::level_enum::warn, QString());
		return false;
	}

    CreateFootprintList();

	QDir directory(m_projectFolder);
	if (!directory.exists())
	{
		emit SendMessage("Directory Doesn't Exist", spdlog::level::level_enum::warn, QString());
		return false;
	}

    missingFootprintList.clear();


    auto const& kicadFiles {directory.entryInfoList(QStringList() << "*.kicad_sch" , QDir::Files)};
	for (auto const& file : kicadFiles)
	{
		emit SendMessage(QString("Checking '%1'").arg(file.fileName()), spdlog::level::level_enum::debug, file.absoluteFilePath());
		CheckSchematic(file.absoluteFilePath());
	}
	return true;
}

void FootprintFinder::CheckSchematic(QString const& schPath)
{
    QFileInfo fileData(schPath);
    QFile inFile(schPath);

	if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		emit SendMessage(QString("Could not Open '%1'").arg(schPath), spdlog::level::level_enum::warn, schPath);
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

        QString footprint = getSchFootprint( line );
        if(footprint.isEmpty())
        {
            continue;
        }
        if(!HasFootprint(footprint))
        {
            if (!missingFootprintList.contains(footprint))
            {
                missingFootprintList.append(footprint);
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

bool FootprintFinder::HasFootprint(QString const& footprint) const
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

void FootprintFinder::CreateFootprintList()
{
    footprintList.clear();

    for(auto const& [level,library] : libraryList)
    {
        for(auto const& lib : library)
        {
            if(footprintList.contains(lib.name))
            {
                SendMessage(QString("Duplicate Libraries %1").arg(lib.name), spdlog::level::level_enum::warn, QString());
                continue;
            }
            QStringList fps;
            if(lib.type == LEGACY_LIB)
            {
                fps = GetLegacyFootprints(lib.url);
            }
            else if(lib.type == KICAD_LIB)
            {
                fps = GetKicadFootprints(lib.url);
            }

            if(!fps.empty())
            {
                footprintList.insert(lib.name, fps);
            }
            else
            {
                emit SendLibraryError(level, lib.name);
            }
        }
    }
}

QStringList FootprintFinder::GetLegacyFootprints(QString const& url) const
{
    QStringList list;

    auto fullPath = updatePath(url);

    //QFileInfo fileData(fullPath);
    QFile inFile(fullPath);

    if(!inFile.exists())
    {
        emit SendResult(QString("'%1' doesnt' exist").arg(fullPath), true);
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

QStringList FootprintFinder::GetKicadFootprints(QString const& url) const
{
    QStringList list;

    auto fullPath = updatePath(url);

    QDir directory(fullPath);
    if(!directory.exists())
    {
        emit SendResult(QString("'%1' doesnt' exist").arg(fullPath), true);
        return list;
    }

    auto const& kicadFiles = directory.entryInfoList(QStringList() << "*.kicad_mod" , QDir::Files );
	for (auto const& file : kicadFiles)
	{
        list.append(file.completeBaseName());
    }

    return list;
}

bool FootprintFinder::FixFootprints(QString const& libfolder)
{
    bool worked{false};
    /*
        Three Steps of "fixing" footprints
        1) Convert absolute paths to relative paths in Footprint Table
        2) Find librarys in library folder and add Footprint Table
        3) IDK 
    */
    if (missingFootprintList.empty())
    {
        CheckSchematics();
    }    

    //find missing libs
    for (auto& library : libraryList[PROJECT_LIB])
    {
        if (library.type == LEGACY_LIB)
        {
            QString path{ updatePath(library.url) };
            if (QFile::exists(path))
            {
                continue;
            }
            QFileInfo const libraryPath{ path };
            QString FoundKicdLibPath = FindRecurseFile(libfolder, QStringList(libraryPath.fileName()));
            if (!FoundKicdLibPath.isEmpty())
            {
                auto newPath{ ConvertToRelativePath(FoundKicdLibPath,libfolder) };
                library.url = newPath;
                worked = true;
            }
        }
        else if (library.type == KICAD_LIB)
        {
            QDir directory(updatePath(library.url));
            if (directory.exists())
            {
                continue;
            }
            QString FoundKicdLibPath = FindRecurseDirectory(libfolder, directory.dirName());
            if (!FoundKicdLibPath.isEmpty())
            {
                auto newPath{ ConvertToRelativePath(FoundKicdLibPath,libfolder) };
                library.url = newPath;
                worked = true;
            }
            //QDir dir(file.dir().absolutePath());
            //dir.cdUp();
            //info.name = dir.dirName();
            //info.url = ConvertToRelativePath(file.dir().absolutePath(), libFolder);
        }  
    }
    if (worked)
    {
        QString localLibPath{ m_projectFolder + "/fp-lib-table" };
        SaveLibraryTable(localLibPath);

        emit SendClearLibrary(PROJECT_LIB);
        libraryList[PROJECT_LIB].clear();
        getProjectLibraries();
        emit SendClearResults();
        CheckSchematics();
    }
    worked = false;

    if (ConvertAllPathsToRelative(libfolder))
    {
        QString localLibPath{ m_projectFolder + "/fp-lib-table" };
        SaveLibraryTable(localLibPath);

        emit SendClearLibrary(PROJECT_LIB);
        libraryList[PROJECT_LIB].clear();
        getProjectLibraries();
        emit SendClearResults();
        CheckSchematics();
    }

    for (auto const& footprint : missingFootprintList)
    {
        if(AttemptToFindFootprintPath(footprint, libfolder))
        {
            worked = true;
        }
    }

    if(worked)
    {
        QString localLibPath {m_projectFolder + "/fp-lib-table"};
        SaveLibraryTable(localLibPath);

        emit SendClearLibrary(PROJECT_LIB);
        libraryList[PROJECT_LIB].clear();
        getProjectLibraries();
        emit SendClearResults();
        CheckSchematics();
    }

    return worked;
}

bool FootprintFinder::AttemptToFindFootprintPath(QString const& footprint, QString const& libraryPath )
{
    if(!footprint.contains(":"))
    {
        return false;
    }
    auto parts {footprint.split(":")};

    QString const LibName{parts[0]};

    QString const Kicd6FootprintName{parts[1] + ".kicad_mod"};

    QString const LibNameFolder{libraryPath + "/" + LibName};

    QString FoundKicdLibPath;

    //first look for libraries in foldse the same as the library name
    QDir libdir(LibNameFolder);
    if(libdir.exists())
    {
        FoundKicdLibPath = FindRecurseFile(LibNameFolder, QStringList(Kicd6FootprintName));
        if(!FoundKicdLibPath.isEmpty())
        {
            //convert from footprint file path to library folder path
            //convert to relative path
            //add to library file
            QFileInfo prettyPath(FoundKicdLibPath);
            auto newPath{ConvertToRelativePath(prettyPath.absolutePath(),libraryPath )};

            emit SendMessage(QString("Found '%1' footprint in '%2'").arg(footprint).arg(newPath), spdlog::level::level_enum::debug, prettyPath.absolutePath());
            AddLibraryPath(parts[0], KICAD_LIB, newPath, PROJECT_LIB);
            return true;
        }

        FoundKicdLibPath = FindRecurseFile(LibNameFolder, {"*.mod"});
        if(!FoundKicdLibPath.isEmpty())
        {
            //legacy
            auto Footprints = GetLegacyFootprints(FoundKicdLibPath);
            if(Footprints.contains(parts[1]))
            {
                //found something
                //convert to relative path
                //add to library file
                auto newPath{ConvertToRelativePath(FoundKicdLibPath,libraryPath)};
                emit SendMessage(QString("Found '%1' footprint in '%2'").arg(footprint).arg(newPath), spdlog::level::level_enum::debug, FoundKicdLibPath);
                AddLibraryPath(parts[0], LEGACY_LIB, newPath,PROJECT_LIB);
                return true;
            }
        }
    }

    FoundKicdLibPath = FindRecurseFile(libraryPath, QStringList(Kicd6FootprintName));

    if(!FoundKicdLibPath.isEmpty())
    {
        //convert from footprint file path to library folder path
        //convert to relative path
        //add to library file
        QFileInfo prettyPath(FoundKicdLibPath);
        auto newPath{ConvertToRelativePath(prettyPath.absolutePath(),libraryPath )};

        emit SendMessage(QString("Found '%1' footprint in '%2'").arg(footprint).arg(newPath), spdlog::level::level_enum::debug, prettyPath.absolutePath());
        AddLibraryPath(parts[0], KICAD_LIB, newPath, PROJECT_LIB);
        return true;
    }

    QStringList const KicdLegacyLibName{LibName + ".mod", parts[1]+ ".mod", LibName + "*.mod", parts[1]+ "*.mod"};
    FoundKicdLibPath = FindRecurseFile(libraryPath, KicdLegacyLibName);
    if(!FoundKicdLibPath.isEmpty())
    {
        //legacy
        auto Footprints = GetLegacyFootprints(FoundKicdLibPath);
        if(Footprints.contains(parts[1]))
        {
            //found something
            //convert to relative path
            //add to library file
            auto newPath{ConvertToRelativePath(FoundKicdLibPath,libraryPath)};
            emit SendMessage(QString("Found '%1' footprint in '%2'").arg(footprint).arg(newPath), spdlog::level::level_enum::debug, FoundKicdLibPath);
            AddLibraryPath(parts[0], LEGACY_LIB, newPath,PROJECT_LIB);
            return true;
        }
    }
    
    return false;
}

QStringList FootprintFinder::GetFootprints(QString const& url, QString const& type) const
{
    if (type == LEGACY_LIB)
    {
        return GetLegacyFootprints(url);
    }
    else if (type == KICAD_LIB)
    {
        return GetKicadFootprints(url);
    }
    return QStringList();
}

LibraryInfo FootprintFinder::DecodeLibraryInfo(QString const& path, QString const& libFolder) const
{
    LibraryInfo info;
    QFileInfo file(path);
    if("mod"== file.suffix().toLower() )
    {
        //info.name = file.baseName();
        info.name = file.dir().dirName();
        info.type = LEGACY_LIB;
        info.url = ConvertToRelativePath(file.absoluteFilePath(), libFolder);
    }
    else if("kicad_mod"== file.suffix().toLower() )
    {
        //info.name = file.dir().dirName();
        //info.name = file.dir().dirName();
        //info.name = info.name.replace(".pretty","");
        QDir dir(file.dir().absolutePath());
        dir.cdUp();
        info.name = dir.dirName();
        info.url = ConvertToRelativePath(file.dir().absolutePath(), libFolder);
        info.type = KICAD_LIB;
    }
    return info;
}