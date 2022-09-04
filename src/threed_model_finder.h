#ifndef THREED_MODEL_FINDER_H
#define THREED_MODEL_FINDER_H

#include "library_info.h"

#include "spdlog/spdlog.h"

#include "library_base.h"

#include <QObject>
#include <QMap>

class ThreeDModelFinder : public LibraryBase
{
	Q_OBJECT

public:
	ThreeDModelFinder();
	~ThreeDModelFinder() {}

	bool CheckPCBs();
	bool FixThreeDModels(QString const& folder);

	void LoadProject(QString const& folder) override;

private:
	void SaveLibraryTable(QString const& fileName) { }

	QString getProjectLibraryPath() const override { return QString(); }
	QString getGlobalLibraryPath() const override { return QString(); }

	LibraryInfo DecodeLibraryInfo(QString const& path, QString const& libFolder) const override { return LibraryInfo(); }

	void CheckPCB(QString const& pcbPath);

	bool AttemptToFixThreeDModelFile(QString const& pcbPath, QString const& libraryPath);

	QString getThreeDModelPath(QString const& line) const;

	QString getPCBReference(QString const& line) const;

	QStringList incorrectThreeDModelFileList;
};

#endif