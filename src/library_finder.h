#ifndef LIBRARY_FINDER_H
#define LIBRARY_FINDER_H

#include "library_info.h"

#include "spdlog/spdlog.h"

#include <QObject>
#include <QMap>

class LibraryFinder : public QObject
{
Q_OBJECT

public:
	LibraryFinder( );
    ~LibraryFinder() {}

	void LoadProject(QString const& folder);
	bool CheckSchematics();
	bool FixFootPrints(QString const& folder);

Q_SIGNALS:
	void SendMessage( QString const& message,  spdlog::level::level_enum llvl) const;

	void AddLibrary( QString const& level, QString const& name, QString const& type, QString const& path) const;

	void SendResult( QString const& message, bool error) const;

private:

	QString m_projectFolder;

	QString getGlobalFootprintsPath() const;
	QString getGlobalFootprintTablePath() const;
	void ParseLibraries(QString const& path, QString const& level);
	void SaveLibraryTable(QString const& fileName);

	QString updatePath(QString path) const;
	QString getLibParamter(QString const& parm, QString const& line) const;
	QString getSchFootprint(QString const& line ) const;
	QString getSchReference(QString const& line ) const;

	void getProjectLibraries();
	void getStockLibraries();

	void CheckSchematic(QString const& schPath);
	void CreateFootprintList();

	bool HasFootPrint(QString const& footprint) const;
	QStringList GetLegacyFootPrints(QString const& url) const;

	QStringList GetKicadFootPrints(QString const& url) const;

	void ConvertToRelativePath(QString const& path, QString const& folder);

	std::vector<LibraryInfo> libraryList;

	QMap<QString,QStringList> footprintList;

	QStringList missingfootprintList;
};

#endif