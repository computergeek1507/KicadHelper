#ifndef FOOTPRINT_FINDER_H
#define FOOTPRINT_FINDER_H

#include "library_info.h"

#include "spdlog/spdlog.h"

#include "library_base.h"

#include <QObject>

class FootprintFinder : public LibraryBase
{
Q_OBJECT

public:
	FootprintFinder( );
    ~FootprintFinder() {}

	bool CheckSchematics();
	bool FixFootPrints(QString const& folder); 

private:
	void SaveLibraryTable(QString const& fileName);

	void getProjectLibraries() override;
	void getGlobalLibraries() override;

	void CheckSchematic(QString const& schPath);
	void CreateFootprintList();

	bool HasFootPrint(QString const& footprint) const;
	QStringList GetLegacyFootPrints(QString const& url) const;

	QStringList GetKicadFootPrints(QString const& url) const;

	bool AttemptToFindFootPrintPath(QString const& footprint, QString const& libraryPath );
	bool ConvertAllPathsToRelative(QString const& libraryPath);

	QMap<QString,QStringList> footprintList;

	QStringList missingfootprintList;
};

#endif