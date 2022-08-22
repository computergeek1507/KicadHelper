#ifndef LIBRARY_FINDER_H
#define LIBRARY_FINDER_H

#include "spdlog/spdlog.h"

#include <QObject>

class LibraryFinder : public QObject
{
Q_OBJECT

public:
	LibraryFinder( );
    ~LibraryFinder() {}

Q_SIGNALS:
	void SendMessage( QString const& message,  spdlog::level::level_enum llvl) const;

	void AddLibrary( QString const& name, QString const& type, QString const& path) const;

private:
	QString GetStockFootprintsPath() const;


};

#endif