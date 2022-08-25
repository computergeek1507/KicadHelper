#ifndef TEXT_REPLACE_H
#define TEXT_REPLACE_H

#include "partinfo.h"

#include "spdlog/spdlog.h"

#include <QString>
#include <QObject>

#include "mapping.h"

class TextReplace : public QObject
{
Q_OBJECT

public:
	TextReplace();
    ~TextReplace() {}

	void LoadJsonFile(const QString& jsonFile);
	void SaveJsonFile(const QString& jsonFile);
	void ImportMappingCSV(QString const& csvFile);
	void SaveMappingCSV(QString const& fileName) const;
	void ClearReplaceList(){ replaceList.clear(); }

	void AddingMapping(QString const& from, QString const& to);
	void RemoveMapping(int index);
	void UpdateMapping(QString const& from, QString const& to, int index);

	std::vector<Mapping> const& getReplaceList() const { return replaceList; }

Q_SIGNALS:
	void SendMessage( QString const& message, spdlog::level::level_enum llvl, QString const& file) const;
	void RedrawTextReplace(bool save) const;
	void UpdateTextRow(int index) const;

private:
	std::vector<Mapping> replaceList;

	void write(QJsonObject& json) const;
	void read(QJsonObject const& json);
};

#endif