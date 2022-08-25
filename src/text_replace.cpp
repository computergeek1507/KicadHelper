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
		{ return elem.from == from; })) {
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

void TextReplace::UpdateMapping(QString const& from, QString const& to, int index) 
{
	//if (std::any_of(replaceList.begin(), replaceList.end(), [&](auto const& elem)
	//	{ return elem.first == from; })) {
	//	return;
	//}
	if (index < 0 || index > replaceList.size())
	{
		return;
	}
	replaceList.at(index).from = from;
	replaceList.at(index).to = to;
	emit UpdateTextRow(index);
}

void TextReplace::LoadJsonFile(const QString& jsonFile)
{
	QFile loadFile(jsonFile);
	//emit SendMessage("Loading json file " + jsonFile, spdlog::level::level_enum::debug);
	if (!loadFile.open(QIODevice::ReadOnly))
	{
		emit SendMessage("Error Opening: " + jsonFile, spdlog::level::level_enum::err, jsonFile);
		return;
	}

	QByteArray saveData = loadFile.readAll();

	QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));

	read(loadDoc.object());

	emit SendMessage("Loaded Json: " + jsonFile, spdlog::level::level_enum::debug, jsonFile);
	emit RedrawTextReplace(false);
	//Q_EMIT RedrawScreen();
}

void TextReplace::SaveJsonFile(const QString& jsonFile)
{
	QFile saveFile(jsonFile);
	//emit SendMessage("Saving json file " + jsonFile, spdlog::level::level_enum::debug);
	if (!saveFile.open(QIODevice::WriteOnly))
	{
		emit SendMessage("Error Saving: " + jsonFile, spdlog::level::level_enum::err, jsonFile);
		return;
	}

	QJsonObject projectObject;
	write(projectObject);
	QJsonDocument saveDoc(projectObject);
	saveFile.write(saveDoc.toJson());
	emit SendMessage("Saved Json to: " + jsonFile, spdlog::level::level_enum::debug, jsonFile);
}

void TextReplace::write(QJsonObject& json) const
{
	QJsonArray mappingArray;
	for (auto const& mapp : replaceList)
	{
		QJsonObject mapObj;
		mapp.write(mapObj);
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
		replaceList.emplace_back(mapObj);
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
				{ return elem.from == from; })) {

				continue;
			}
			replaceList.emplace_back(from, kicad_utils::CleanQuotes(row[1].c_str()));
		}
	}
	emit RedrawTextReplace(true);
}

void TextReplace::SaveMappingCSV(QString const& fileName) const
{
	QFile outFile(fileName);
	if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		emit SendMessage(QString("Could not Open '%1'").arg(fileName), spdlog::level::level_enum::warn, fileName);
		return;
	}
	QTextStream out(&outFile);
	for (auto const& replace : replaceList)
	{
		out << replace.asString() << "\n";
	}	
	outFile.close();
	emit SendMessage(QString("Saved Mapping CSV to '%1'").arg(outFile.fileName()), spdlog::level::level_enum::debug, outFile.fileName());
}