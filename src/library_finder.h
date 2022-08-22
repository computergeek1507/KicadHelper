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

	void LoadProject(QString const& folder);

Q_SIGNALS:
	void SendMessage( QString const& message,  spdlog::level::level_enum llvl) const;

	void AddLibrary( QString const& name, QString const& type, QString const& path) const;

private:

	QString m_projectFolder;

	QString getGlobalFootprintsPath() const;
	QString getGlobalFootprintTablePath() const;
	void ParseLibraries(QString const& path) const;

	QString updatePath(QString path) const;
	QString getLibParamter(QString const& line, QString const& parm) const;

	void getProjectLibraries() const;
	void getStockLibraries() const;
};

#endif