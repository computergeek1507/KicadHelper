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
 

Q_SIGNALS:
	void AddLibrary( QString const& level, QString const& name, QString const& type, QString const& path) const;

	void ClearLibrary( QString const& level) const;

	void SendResult( QString const& message, bool error) const;
	void ClearResults() const;

private:

	void SaveLibraryTable(QString const& fileName);

	QString getSchFootprint(QString const& line ) const;
	QString getSchReference(QString const& line ) const;

	void getProjectLibraries() override;
	void getGlobalLibraries() override;

	void CheckSchematic(QString const& schPath);
	void CreateFootprintList();

	bool HasFootPrint(QString const& footprint) const;
	QStringList GetLegacyFootPrints(QString const& url) const;

	QStringList GetKicadFootPrints(QString const& url) const;

	bool AttemptToFindFootPrintPath(QString const& footprint, QString const& libraryPath );
	bool ConvertAllPathsToRelative(QString const& libraryPath);

	void AddLibraryPath(QString name, QString type, QString url, QString const& level);
	


	QMap<QString,QStringList> footprintList;

	QStringList missingfootprintList;
};

#endif