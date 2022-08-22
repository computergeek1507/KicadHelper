#include "library_finder.h"

#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QCoreApplication>
#include <QRegularExpression>

LibraryFinder::LibraryFinder()
{

}
void LibraryFinder::LoadProject(QString const& folder)
{
    m_projectFolder = folder;
    getProjectLibraries();
    getStockLibraries();
}

void LibraryFinder::getProjectLibraries() const
{
    QString localLibPath = m_projectFolder + "fp-lib-table";
    if(QFile::exists(localLibPath))
    {
        ParseLibraries(localLibPath);
    }
}

void LibraryFinder::getStockLibraries() const
{
    if(QFile::exists(getGlobalFootprintTablePath()))
    {
        ParseLibraries(getGlobalFootprintTablePath());
    }
}

void LibraryFinder::ParseLibraries(QString const& path) const
{
    //\((name)\s(\S+)\)
    //\((\w+)\s(\S+)\)
    QRegularExpression nameRx(R"(\((\w+)\s(\S+)\))");
    QRegularExpression typeRx(R"(\((\w+)\s(\S+)\))");
    QRegularExpression typeRx(R"(\((\w+)\s(\S+)\))");

	QFile inFile(path);

	if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		emit SendMessage(QString("Cannot Open %1").arg(path), spdlog::level::level_enum::warn);
		return;
	}
	QTextStream in(&inFile);
	while (!in.atEnd())
	{
		QString line = in.readLine();
        QString name = getLibParamter("name", line);
        QString type = getLibParamter("type", line);
        QString uri = getLibParamter("uri", line);

	}
	inFile.close();

}

QString LibraryFinder::updatePath(QString path) const
{
    path = path.replace("${KICAD6_FOOTPRINT_DIR}", getGlobalFootprintsPath() );
    path = path.replace("${KIPRJMOD}", m_projectFolder);
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
    return R"( "/usr/share/kicad/footprints/)" ;
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
    return R"( "/usr/share/kicad/template/fp-lib-table)" ;
#endif
    //C:\Program Files\KiCad\6.0\share\kicad\template\fp-lib-table
}

QString LibraryFinder::getLibParamter(QString const& line, QString const& parm) const
{
   int stInd =  line.indexOf("(" + parm );
   int endInd =  line.indexOf(")",stInd );

   if(stInd == endInd || stInd ==-1 || endInd == -1)
   {
       return QString();
   }
   int nstr {stInd + parm.length() + 1};
   QString value = line.mid(nstr, endInd - nstr);
   return value;
}