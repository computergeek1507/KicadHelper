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

QString SymbolFinder::getProjectLibraryPath() const
{
   return m_projectFolder + "/sym-lib-table";
}

QString SymbolFinder::getGlobalLibraryPath() const
{
    return getGlobalSymbolTablePath();
}

void SymbolFinder::SaveLibraryTable(QString const& fileName)
{
    QFile outFile(fileName);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        emit SendMessage(QString("Could not Open '%1'").arg(fileName), spdlog::level::level_enum::warn, fileName);
        return;
    }
    QTextStream out(&outFile);

    out << "(sym_lib_table\n";
    for (auto const& library : libraryList[PROJECT_LIB])
    {
        out << library.asString() << "\n";
    }
    out << ")\n";
    outFile.close();
    emit SendMessage(QString("Saved Library Table to '%1'").arg(outFile.fileName()), spdlog::level::level_enum::debug, fileName);
}

bool SymbolFinder::CheckSchematics()
{
	if (libraryList.empty())
	{
		emit SendMessage("Library List is empty", spdlog::level::level_enum::warn, QString());
		return false;
	}

    CreateSymbolList();

	QDir directory(m_projectFolder);
	if (!directory.exists())
	{
		emit SendMessage("Directory Doesn't Exist", spdlog::level::level_enum::warn, QString());
		return false;
	}

    missingSymbolList.clear();
    rescueSymbolList.clear();

    auto const& kicadFiles {directory.entryInfoList(QStringList() << "*.kicad_sch" , QDir::Files)};
	for (auto const& file : kicadFiles)
	{
		emit SendMessage(QString("Checking '%1'").arg(file.fileName()), spdlog::level::level_enum::debug, file.absoluteFilePath());
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
		emit SendMessage(QString("Could not Open '%1'").arg(schPath), spdlog::level::level_enum::warn, schPath);
		return;
	}
    bool errorFound{false};
    bool partSection{false};
    //int lineCount{0};

    //QString reference;
    QString symbol;
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
        QString s = getSchSymbol(line);
        if(!s.isEmpty())
        {
            symbol = s;
            continue;
        }

        QString ref = getSchReference( line);
        if(ref.isEmpty())
        {
           continue;
        }

       
        if(!HasSymbol(symbol))
        {
            if (!missingSymbolList.contains(symbol))
            {
                missingSymbolList.append(symbol);
            }
            emit SendResult(QString("'%1':'%2' was not found in '%3'").arg(ref).arg(symbol).arg(fileData.fileName()),true);
            errorFound = true;
        }

        if(symbol.contains("-rescue"))
        {
            if (!rescueSymbolList.contains(symbol))
            {
                rescueSymbolList.append(symbol);
            }
            emit SendResult(QString("'%1':'%2' is a rescue symbol '%3'").arg(ref).arg(symbol).arg(fileData.fileName()),false);
            //errorFound = true;
        }
        symbol.clear();
	}
	inFile.close();

    if(!errorFound)
    {
        emit SendResult(QString("'%1' is Good").arg(fileData.fileName()), false);
    }
}

bool SymbolFinder::HasSymbol(QString const& symbol) const
{
    if(!symbol.contains(":"))
    {
        return false;
    }
    auto parts {symbol.split(":")};

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

    for(auto const& [level, library] : libraryList)
    {
        for(auto const& lib : library)
        {
            if(SymbolList.contains(lib.name))
            {
                SendMessage(QString("Duplicate Libraries %1").arg(lib.name), spdlog::level::level_enum::warn, QString());
                continue;
            }
            QStringList fps;
            if(lib.type == LEGACY_LIB)
            {
                fps = GetLegacySymbols(lib.url);
            }
            else if(lib.type == KICAD_LIB)
            {
                fps = GetKicadSymbols(lib.url);
            }

            if(!fps.empty())
            {
                SymbolList.insert(lib.name, fps);
            }
            else 
            {
                emit SendLibraryError(level, lib.name);
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

QStringList SymbolFinder::GetKicadSymbols(QString const& url) const
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

        QString symbol = getLibSymbol( line);
        if(symbol.isEmpty())
        {
            continue;
        } 
        if (symbol.contains(":"))
        {
            symbol = symbol.split(":")[1];
        }

        list.append(symbol);
	}
	inFile.close();
    return list;
}

bool SymbolFinder::FixSymbols(QString const& libfolder)
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

    //find missing libs
    for (auto& library : libraryList[PROJECT_LIB])
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

    if (worked)
    {
        SaveLibraryTable(getProjectLibraryPath());

        emit SendClearLibrary(PROJECT_LIB);
        libraryList[PROJECT_LIB].clear();
        getProjectLibraries();
        emit SendClearResults();
        CheckSchematics();
    }
    worked = false;

    if(ConvertAllPathsToRelative(libfolder))
    {
        SaveLibraryTable(getProjectLibraryPath());

        emit SendClearLibrary(PROJECT_LIB);
        libraryList[PROJECT_LIB].clear();
        getProjectLibraries();
        emit SendClearResults();
        CheckSchematics();
    }

    for (auto const& footprint : missingSymbolList)
    {
        if(AttemptToFindSymbolPath(footprint, libfolder))
        {
            worked = true;
        }
    }

    if(worked)
    {
        SaveLibraryTable(getProjectLibraryPath());

        emit SendClearLibrary(PROJECT_LIB);
        libraryList[PROJECT_LIB].clear();
        getProjectLibraries();
        emit SendClearResults();
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

    QString const LibName{parts[0]};

    QString const Kicd6SymbolName{LibName + ".kicad_sym"};

    QString const LibNameFolder{libraryPath + "/"+ LibName};

    QString FoundKicdLibPath;

    //first look for libraries in foldse the same as the library name
    QDir libdir(LibNameFolder);
    if(libdir.exists())
    {
        FoundKicdLibPath = FindRecurseFile(LibNameFolder, QStringList(Kicd6SymbolName));
        if(!FoundKicdLibPath.isEmpty())
        {
            auto symbols = GetKicadSymbols(FoundKicdLibPath);
            if(symbols.contains(parts[1]))
            {
                //found something
                //convert to relative path
                //add to library file

                auto newPath{ConvertToRelativePath(FoundKicdLibPath,libraryPath )};

                emit SendMessage(QString("Found '%1' symbol in '%2'").arg(footprint).arg(newPath), spdlog::level::level_enum::debug, FoundKicdLibPath);
                AddLibraryPath(parts[0], KICAD_LIB, newPath, PROJECT_LIB);
                return true;
            }
        }

        FoundKicdLibPath = FindRecurseFile(LibNameFolder, {"*.lib"});
        if(!FoundKicdLibPath.isEmpty())
        {
            //legacy
            auto Footprints = GetLegacySymbols(FoundKicdLibPath);
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

    FoundKicdLibPath = FindRecurseFile(libraryPath, QStringList(Kicd6SymbolName));

    if(!FoundKicdLibPath.isEmpty())
    {
        auto symbols = GetKicadSymbols(FoundKicdLibPath);
        if(symbols.contains(parts[1]))
        {
            //found something
            //convert to relative path
            //add to library file

            auto newPath{ConvertToRelativePath(FoundKicdLibPath, libraryPath)};

            emit SendMessage(QString("Found '%1' symbol in '%2'").arg(footprint).arg(newPath), spdlog::level::level_enum::debug, FoundKicdLibPath);
            AddLibraryPath(parts[0], KICAD_LIB, newPath, PROJECT_LIB);
            return true;
        }
    }

    QStringList const KicdLegacyLibName{LibName + ".lib", parts[1]+ ".lib", LibName + "*.lib", parts[1]+ "*.lib"};
    FoundKicdLibPath = FindRecurseFile(libraryPath, KicdLegacyLibName);
    if(!FoundKicdLibPath.isEmpty())
    {
        //legacy
        auto Footprints = GetLegacySymbols(FoundKicdLibPath);
        if(Footprints.contains(parts[1]))
        {
            //found something
            //convert to relative path
            //add to library file
            auto newPath{ConvertToRelativePath(FoundKicdLibPath,libraryPath)};
            emit SendMessage(QString("Found '%1' symbol in '%2'").arg(footprint).arg(newPath), spdlog::level::level_enum::debug, FoundKicdLibPath);
            AddLibraryPath(parts[0], LEGACY_LIB, newPath,PROJECT_LIB);
            return true;
        }
    }
    
    return false;
}

QStringList SymbolFinder::GetSymbols(QString const& url, QString const& type) const 
{
    if (type == LEGACY_LIB)
    {
        return GetLegacySymbols(url);
    }
    else if (type == KICAD_LIB)
    {
        return GetKicadSymbols(url);
    }
    return QStringList();
}

LibraryInfo SymbolFinder::DecodeLibraryInfo(QString const& path, QString const& libFolder) const
{
    LibraryInfo info;
    QFileInfo file(path);
    //info.name = file.baseName();
    info.name = file.dir().dirName();
    info.url = ConvertToRelativePath(file.absoluteFilePath(), libFolder);
    if("lib"== file.suffix().toLower() )
    {
        info.type = LEGACY_LIB;
    }
    else if("kicad_sym"== file.suffix().toLower() )
    {
        info.type = KICAD_LIB;
    }
    return info;
}

