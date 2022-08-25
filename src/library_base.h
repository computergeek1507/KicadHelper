#ifndef LIBRARY_BASE_H
#define LIBRARY_BASE_H

#include "spdlog/spdlog.h"

#include "library_info.h"

#include <QObject>
#include <QMap>

constexpr const char* PROJECT_LIB = "Project";
constexpr const char* GLOBAL_LIB = "Global";

//constexpr std::string_view PROJ_FOLDER = "${KIPRJMOD}";

class LibraryBase : public QObject
{
Q_OBJECT

public:
	LibraryBase() {}
    virtual ~LibraryBase() {}

	QString updatePath(QString path) const;
	virtual void LoadProject(QString const& folder);

Q_SIGNALS:
	void SendMessage( QString const& message,  spdlog::level::level_enum llvl) const;

	void AddLibrary(QString const& level, QString const& name, QString const& type, QString const& path) const;
	void ClearLibrary(QString const& level) const;

	void SendResult(QString const& message, bool error) const;
	void ClearResults() const;

protected:
	virtual void getProjectLibraries() = 0;
	virtual void getGlobalLibraries() = 0;

	void ParseLibraries(QString const& path, QString const& level);
	virtual void SaveLibraryTable(QString const& fileName) = 0;

	QString getGlobalKicadDataPath() const;
	QString getGlobalTemplatePath() const;
	QString getGlobalFootprintPath() const;
	QString getGlobalSymbolPath() const;
	QString getGlobalFootprintTablePath() const;
	QString getGlobalSymbolTablePath() const;

	QString FindRecurseDirectory(const QString& startDir, const QString& dirName) const;
	QString FindRecurseFile(const QString& startDir, const QStringList& fileName) const;

	QString ConvertToRelativePath(QString const& ogpath, QString const& libraryPath);
	QString getLibParamter(QString const& parm, QString const& line ) const;

	bool ConvertAllPathsToRelative(QString const& libraryPath);

	void AddLibraryPath(QString name, QString type, QString url, QString const& level);

	QString m_projectFolder;

	QMap<QString, std::vector<LibraryInfo>> libraryList;
};

#endif