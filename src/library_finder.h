#ifndef LIBRARY_FINDER_H
#define LIBRARY_FINDER_H

#include "library_info.h"

#include "spdlog/spdlog.h"

#include <QObject>
#include <QMap>

constexpr const char* PROJECT_LIB = "Project";
constexpr const char* GLOBAL_LIB = "Global";

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

	void ClearLibrary( QString const& level) const;

	void SendResult( QString const& message, bool error) const;
	void ClearResults() const;

private:


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

	bool AttemptToFindFootPrintPath(QString const& footprint, QString const& libraryPath );
	bool ConvertAllPathsToRelative(QString const& libraryPath);

	QString FindRecurseDirectory(const QString& startDir, const QString& dirName) const;
	QString FindRecurseFile(const QString& startDir, const QStringList& fileName) const;

	QString ConvertToRelativePath(QString const& ogpath, QString const& libraryPath);

	void AddLibraryPath(QString name, QString type, QString url, QString const& level);
	
	QString m_projectFolder;
	QMap<QString,std::vector<LibraryInfo>> libraryList;

	QMap<QString,QStringList> footprintList;

	QStringList missingfootprintList;
};

#endif