#ifndef LIBRARY_BASE_H
#define LIBRARY_BASE_H

#include "spdlog/spdlog.h"

#include "library_info.h"

#include <QObject>
#include <QMap>

constexpr const char* PROJECT_LIB = "Project";
constexpr const char* GLOBAL_LIB = "Global";

constexpr const char* LEGACY_LIB = "Legacy";
constexpr const char* KICAD_LIB = "KiCad";

//constexpr std::string_view PROJ_FOLDER = "${KIPRJMOD}";

class LibraryBase : public QObject
{
Q_OBJECT

public:
	LibraryBase() {}
    virtual ~LibraryBase() {}

	QString updatePath(QString path) const;
	virtual void LoadProject(QString const& folder);

	virtual void ChangeLibraryName(QString const& oldName, QString const& newName, int row);
	virtual void ChangeLibraryPath(QString const& name, QString const& newPath, int row);
Q_SIGNALS:
	void SendMessage( QString const& message,  spdlog::level::level_enum llvl, QString const& file) const;

	void AddLibrary(QString const& level, QString const& name, QString const& type, QString const& descr, QString const& path) const;
	void ClearLibrary(QString const& level) const;

	void UpdateLibraryRow(QString const& level, QString const& name, QString const& type, QString const& descr, QString const& path, int row) const;

	void SendResult(QString const& message, bool error) const;
	void ClearResults() const;

protected:
	void ParseLibraries(QString const& path, QString const& level);
	virtual void SaveLibraryTable(QString const& fileName) = 0;
	virtual QString getProjectLibraryPath() const = 0;
	virtual QString getGlobalLibraryPath() const = 0;

	void getProjectLibraries();
	void getGlobalLibraries();

	QString getGlobalKicadDataPath() const;
	QString getGlobalTemplatePath() const;
	QString getGlobalFootprintPath() const;
	QString getGlobalSymbolPath() const;
	QString getGlobalFootprintTablePath() const;
	QString getGlobalSymbolTablePath() const;

	QString FindRecurseDirectory(const QString& startDir, const QString& dirName) const;
	QString FindRecurseFile(const QString& startDir, const QStringList& fileName) const;
	QStringList FindRecurseFiles(const QString& startDir, const QStringList& fileNames) const;

	QString ConvertToRelativePath(QString const& ogpath, QString const& libraryPath);

	QString getSchSymbol(QString const& line ) const;
	QString getSchFootprint(QString const& line ) const;
	QString getSchReference(QString const& line ) const;
	QString getLibSymbol(QString const& line ) const;

	QString getLibParamter(QString const& parm, QString const& line ) const;

	bool ConvertAllPathsToRelative(QString const& libraryPath);

	void AddLibraryPath(QString name, QString type, QString url, QString const& level);

	QString m_projectFolder;

	QMap<QString, std::vector<LibraryInfo>> libraryList;
};

#endif