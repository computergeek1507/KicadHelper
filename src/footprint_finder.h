#ifndef FOOTPRINT_FINDER_H
#define FOOTPRINT_FINDER_H

#include "library_info.h"

#include "spdlog/spdlog.h"

#include "library_base.h"

#include <QObject>
#include <QMap>

class FootprintFinder : public LibraryBase
{
Q_OBJECT

public:
	FootprintFinder( );
    ~FootprintFinder() {}

	bool CheckSchematics();
	bool FixFootprints(QString const& libfolder);
	QStringList GetFootprints(QString const& url, QString const& type) const;

private:
	void SaveLibraryTable(QString const& fileName);

	QString getProjectLibraryPath() const override;
	QString getGlobalLibraryPath() const override;

	LibraryInfo DecodeLibraryInfo(QString const& path, QString const& libFolder) const override;

	void CheckSchematic(QString const& schPath);
	void CreateFootprintList();

	bool HasFootprint(QString const& footprint) const;
	QStringList GetLegacyFootprints(QString const& url) const;

	QStringList GetKicadFootprints(QString const& url) const;

	bool AttemptToFindFootprintPath(QString const& footprint, QString const& libraryPath );

	QMap<QString,QStringList> footprintList;

	QStringList missingFootprintList;
};

#endif