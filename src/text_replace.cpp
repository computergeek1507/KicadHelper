#include "text_replace.h"

#include "csvparser.h"

#include "kicad_utils.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

TextReplace::TextReplace()
{

}

void TextReplace::AddingMapping(QString const& from, QString const& to)
{
	if (std::any_of(replaceList.begin(), replaceList.end(), [&](auto const& elem)
		{ return elem.first == from; })) {
		return;
	}
	replaceList.emplace_back(from, to);
	emit RedrawTextReplace(true);
}

void TextReplace::RemoveMapping(int index)
{
	if (index < 0 || index > replaceList.size())
	{
		return;
	}
	replaceList.erase(replaceList.begin() + index);
	emit RedrawTextReplace(true);
}

void TextReplace::LoadJsonFile(const QString& jsonFile)
{
	QFile loadFile(jsonFile);
	//emit SendMessage("Loading json file " + jsonFile, spdlog::level::level_enum::debug);
	if (!loadFile.open(QIODevice::ReadOnly))
	{
		emit SendMessage("Error Opening: " + jsonFile, spdlog::level::level_enum::err);
		return;
	}

	QByteArray saveData = loadFile.readAll();

	QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));

	read(loadDoc.object());

	emit SendMessage("Loaded Json: " + jsonFile, spdlog::level::level_enum::debug);
	emit RedrawTextReplace(false);
	//Q_EMIT RedrawScreen();
}

void TextReplace::SaveJsonFile(const QString& jsonFile)
{
	QFile saveFile(jsonFile);
	//emit SendMessage("Saving json file " + jsonFile, spdlog::level::level_enum::debug);
	if (!saveFile.open(QIODevice::WriteOnly))
	{
		emit SendMessage("Error Saving: " + jsonFile, spdlog::level::level_enum::err);
		return;
	}

	QJsonObject projectObject;
	write(projectObject);
	QJsonDocument saveDoc(projectObject);
	saveFile.write(saveDoc.toJson());
	emit SendMessage("Saved Json to: " + jsonFile, spdlog::level::level_enum::debug);
}

void TextReplace::write(QJsonObject& json) const
{
	QJsonArray mappingArray;
	for (auto const& [from, to] : replaceList)
	{
		QJsonObject mapObj;
		mapObj["from"] = from;
		mapObj["to"] = to;
		mappingArray.append(mapObj);
	}
	json["mappings"] = mappingArray;
}

void TextReplace::read(QJsonObject const& json)
{
	replaceList.clear();

	QJsonArray mappingArray = json["mappings"].toArray();
	for (auto const& mapp : mappingArray)
	{
		QJsonObject mapObj = mapp.toObject();
		replaceList.emplace_back(mapObj["from"].toString(), mapObj["to"].toString());
	}
}

void TextReplace::ImportMappingCSV(QString const& csvFile)
{
	auto lines{ csvparser::parsefile(csvFile.toStdString()) };

	for (auto const& row : lines)
	{
		if (row.size() > 1)
		{
			auto from{ kicad_utils::CleanQuotes(row[0].c_str()) };
			if (std::any_of(replaceList.begin(), replaceList.end(), [&](auto const& elem)
				{ return elem.first == from; })) {

				continue;
			}
			replaceList.emplace_back(from, kicad_utils::CleanQuotes(row[1].c_str()));
		}
	}
	emit RedrawTextReplace(true);
}