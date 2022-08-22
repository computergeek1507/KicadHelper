#include "library_finder.h"

#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QCoreApplication>

LibraryFinder::LibraryFinder()
{

}

QString LibraryFinder::GetStockFootprintsPath() const
{
    //C:\Program Files\KiCad\6.0\share\kicad\footprints

#if defined( Q_OS_DARWIN )
    return R"(kicad.app/Contents/SharedSupport/footprints)";
#elif defined( Q_OS_WIN )
#ifdef _DEBUG
    return R"(C:/Program Files/KiCad/6.0/share\kicad/footprints/)";
#else
    QFileInfo root( QCoreApplication::applicationDirPath() +  ( "/usr/share/kicad/footprints/" ) );
    return root.absoluteFilePath();
#endif 
#else
    return R"( "/usr/share/kicad/footprints/)" ;
#endif
}