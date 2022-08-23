#ifndef TEXT_REPLACE_H
#define TEXT_REPLACE_H

#include "partinfo.h"

#include "spdlog/spdlog.h"

#include <QString>
#include <QObject>

class TextReplace : public QObject
{
Q_OBJECT

public:
	TextReplace( );
    ~TextReplace() {}

	void LoadJsonFile(const QString& jsonFile);
	void SaveJsonFile(const QString& jsonFile);
	void ImportMappingCSV(QString const& csvFile);
	void ClearReplaceList(){ replaceList.clear(); }

	void AddingMapping(QString const& from, QString const& to);
	void RemoveMapping(int index);

	std::vector<std::pair<QString, QString>> const& getReplaceList() const { return replaceList; }

Q_SIGNALS:
	void SendMessage( QString const& message, spdlog::level::level_enum llvl) const;
	void RedrawTextReplace(bool save) const;

private:
	std::vector<std::pair<QString, QString>> replaceList;

	void write(QJsonObject& json) const;
	void read(QJsonObject const& json);
};

#endif