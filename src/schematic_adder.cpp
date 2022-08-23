#include "schematic_adder.h"

#include "csvparser.h"

#include "kicad_utils.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

SchematicAdder::SchematicAdder()
{

}

void SchematicAdder::AddPart(PartInfo part)
{
	if (std::any_of(partList.begin(), partList.end(), [&](auto const& elem)
		{ return elem.value == part.value && elem.footPrint == part.footPrint; })) {

		return;
	}
	partList.push_back(part);
	emit RedrawPartList(true);
}

void SchematicAdder::RemovePart(int index)
{
	if (index < 0 || index > partList.size())
	{
		return;
	}
	partList.erase(partList.begin() + index);
	emit RedrawPartList(true);
}

bool SchematicAdder::AddPartNumbersToSchematics(QString const& schDir) const
{
	if (partList.empty())
	{
		emit SendMessage("Part List is empty", spdlog::level::level_enum::warn);
		return false;
	}
	QDir directory(schDir);
	if (!directory.exists())
	{
		emit SendMessage("Directory Doesn't Exist", spdlog::level::level_enum::warn);
		return false;
	}

	auto const& kicadFiles {directory.entryInfoList(QStringList() << "*.kicad_sch" , QDir::Files)};
	for (auto const& file : kicadFiles)
	{
		emit SendMessage(QString("Updating PN's in '%1'").arg(file.fileName()), spdlog::level::level_enum::debug);
		UpdateSchematic(file.absoluteFilePath());
	}
	return true;
}

void SchematicAdder::UpdateSchematic(QString const& schPath) const
{
	//Regex to look through schematic property, if we hit the pin section without finding a LCSC property, add it
	//keep track of property ids and Reference property location to use with new LCSC property
	// R"(\\(property\\s\\"(.*)\\"\s\\"(.*)\\"\s\\(id\\s(\\d +)\\)\\s\\(at\\s(-?\\d+.\\d+\\s-?\\d+.\\d+)\s\\d+\\)"
	//\(property\s\"(.*)\"\s\"(.*)\"\s\(id\s(\d+)\)\s\(at\s(-?\d+(?:.\d+)?\s-?\d+(?:.\d+)?)\s\d+\)
	QRegularExpression propRx(
		R"(\(property\s\"(.*)\"\s\"(.*)\"\s\(id\s(\d+)\)\s\(at\s(-?\d+(?:.\d+)?\s-?\d+(?:.\d+)?)\s\d+\))"
	);
	QRegularExpression pinRx(R"(\(pin\s\"(.*)\"\s\()");

	int lastID{ -1 };
	QString lastLoc;
	QString lastDigikey;
	QString newDigikey;
	QString lastLcsc;
	QString newLcsc;
	QString lastMPN;
	QString newMPN;
	QString lastRef;
	QString lastValue;

	std::vector<QString> lines;
	std::vector<QString> newlines;

	QFile inFile(schPath);

	if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		emit SendMessage(QString("Could not Open '%1'").arg(schPath), spdlog::level::level_enum::warn);
		return;
	}
	QTextStream in(&inFile);
	while (!in.atEnd())
	{
		lines.emplace_back(in.readLine());
	}
	inFile.close();

	bool partSection{false};
	for (auto const& line : lines)
	{
		QString outLine = line;
		QRegularExpressionMatch m = propRx.match(line);

		if(line.contains("(symbol (lib_id"))
		{
			partSection = true;
		}

		if (m.hasMatch() && line.startsWith("    (property") && partSection)
		{
			auto key = m.captured(1);
			auto	value = m.captured(2);
			auto slastID = m.captured(3);
			lastID = m.captured(3).toInt();

			if (key == "Digi-Key_PN")
			{
				lastDigikey = value;
				if (lastDigikey != newDigikey && !newDigikey.isEmpty())
				{
					emit SendMessage(QString("Updating '%1' Digikey to '%2'").arg(lastRef).arg(newDigikey), spdlog::level::level_enum::debug);
					outLine.replace("\"" + lastDigikey + "\"", "\"" + newDigikey + "\"");
					lastDigikey = newDigikey;
				}
			}
			if (key == "LCSC")
			{
				lastLcsc = value;
				if (lastLcsc != newLcsc && !newLcsc.isEmpty())
				{
					emit SendMessage(QString("Updating '%1' LCSC to '%2'").arg(lastRef).arg(newLcsc), spdlog::level::level_enum::debug);
					outLine.replace("\"" + lastLcsc + "\"", "\"" + newLcsc + "\"");
					lastLcsc = newLcsc;
				}
			}
			if (key == "MPN")
			{
				lastMPN = value;
				if (lastMPN != newMPN && !newMPN.isEmpty())
				{
					emit SendMessage(QString("Updating '%1' MPN to '%2'").arg(lastRef).arg(newMPN), spdlog::level::level_enum::debug);
					outLine.replace("\"" + lastMPN + "\"", "\"" + newMPN + "\"");
					lastMPN = newMPN;
				}
			}

			if (key == "Reference")
			{
				lastLoc = m.captured(4);
				lastRef = value;
			}
			if (key == "Value")
			{
				lastValue = value;
				//for (auto const& part : partList) {
				//	if (value == part.value)
				//	{
				//		newDigikey = part.digikey;
				//		newLcsc = part.lcsc;
				//		newMPN = part.mpn;
				//		break;
				//	}
				//}
			}
			if (key == "Footprint")
			{
				bool found{false};
				for (auto const& part : partList)
				{
					if (lastValue == part.value && value == part.footPrint)
					{
						newDigikey = part.digikey;
						newLcsc = part.lcsc;
						newMPN = part.mpn;
						found = true;
						emit SendMessage(QString("Part Found based on FootPrint '%1':'%2'").arg(part.value).arg(part.footPrint), spdlog::level::level_enum::debug);
						break;
					}
				}
				if(!found)
				{
					for (auto const& part : partList)
					{
						if (lastValue == part.value)
						{
							newDigikey = part.digikey;
							newLcsc = part.lcsc;
							newMPN = part.mpn;
							emit SendMessage(QString("Part Found based on Value '%1':'%2'").arg(part.value).arg(part.footPrint), spdlog::level::level_enum::debug);
							break;
						}
					}
				}
			}
		}

		QRegularExpressionMatch pm = pinRx.match(line);

		bool addDigi = lastDigikey.isEmpty() && !newDigikey.isEmpty();
		bool addLcsc = lastLcsc.isEmpty() && !newLcsc.isEmpty();
		bool addMPN = lastMPN.isEmpty() && !newMPN.isEmpty();

		if (pm.hasMatch()) 
		{
			if ((addDigi || addLcsc || addMPN) && !lastLoc.isEmpty() && lastID != -1)
			{
				int newid = lastID + 1;
				if (addDigi)
				{
					emit SendMessage(QString("Adding '%1' DigiKey '%2'").arg(lastRef).arg(newDigikey), spdlog::level::level_enum::debug);
					QString newTxt = QString("    (property \"Digi-Key_PN\" \"%1\" (id %2) (at %3 0)").arg(newDigikey).arg(newid).arg(lastLoc);
					newlines.push_back(newTxt);
					newlines.push_back("      (effects (font (size 1.27 1.27)) hide)");
					newlines.push_back("    )");
					newid++;
				}
				if (addLcsc)
				{
					emit SendMessage(QString("Adding '%1' LCSC '%2'").arg(lastRef).arg(newLcsc), spdlog::level::level_enum::debug);
					QString newTxt = QString("    (property \"LCSC\" \"%1\" (id %2) (at %3 0)").arg(newLcsc).arg(newid).arg(lastLoc);
					newlines.push_back(newTxt);
					newlines.push_back("      (effects (font (size 1.27 1.27)) hide)");
					newlines.push_back("    )");
					newid++;
				}
				if (addMPN)
				{
					emit SendMessage(QString("Adding '%1' MPN '%2'").arg(lastRef).arg(newMPN), spdlog::level::level_enum::debug);
					QString newTxt = QString("    (property \"MPN\" \"%1\" (id %2) (at %3 0)").arg(newMPN).arg(newid).arg(lastLoc);
					newlines.push_back(newTxt);
					newlines.push_back("      (effects (font (size 1.27 1.27)) hide)");
					newlines.push_back("    )");
					newid++;
				}
			}
			lastDigikey = "";
			newDigikey = "";
			lastLcsc = "";
			newLcsc = "";
			lastMPN = "";
			newMPN = "";
			lastID = -1;
			lastLoc = "";
			lastRef = "";
		}
		newlines.push_back(outLine);
	}

	try
	{
		if (QFile::exists(schPath + "_old"))
		{
			QFile::remove(schPath + "_old");
		}

		QFile::rename(schPath, schPath + "_old");
	}
	catch (std::exception ex)
	{
		emit SendMessage(QString("Could not Create '%1_old'").arg(schPath), spdlog::level::level_enum::warn);
	}

	QFile outFile(schPath);
	if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		emit SendMessage(QString("Could not Open '%1'").arg(schPath), spdlog::level::level_enum::warn);
		return;
	}
	QTextStream out(&outFile);
	for (auto const& line : newlines) {
		out << line << "\n";
	}	
	outFile.close();
}

void SchematicAdder::LoadJsonFile(const QString& jsonFile)
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
	//Q_EMIT RedrawScreen();
	emit RedrawPartList(false);
}

void SchematicAdder::SaveJsonFile(const QString& jsonFile)
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

void SchematicAdder::write(QJsonObject& json) const
{
	QJsonArray partArray;
	for (auto const& part : partList)
	{
		QJsonObject partObj;
		part.write(partObj);
		partArray.append(partObj);
	}
	json["parts"] = partArray;
}

void SchematicAdder::read(QJsonObject const& json)
{
	partList.clear();

	QJsonArray partArray = json["parts"].toArray();
	for (auto const& part : partArray)
	{
		QJsonObject npcObject = part.toObject();
		partList.emplace_back(npcObject);
	}
}

void SchematicAdder::ImportPartNumerCSV(QString const& csvFile)
{
	auto lines{ csvparser::parsefile(csvFile.toStdString()) };

	int valuCol{ 0 };
	int fpCol{ 1 };
	int lcscCol{ 3 };
	bool lcscBOM{ false };
	for (auto const& line : lines)
	{
		if (line.size() < 3)
		{
			continue;
		}
		QString lcsc;
		QString mpn;
		QString digi;
		if (line.size() > 3)
		{
			lcsc = line[3].c_str();
			lcsc = kicad_utils::CleanQuotes(lcsc);
		}
		if (line.size() > 4)
		{
			mpn = line[4].c_str();
			mpn = kicad_utils::CleanQuotes(mpn);
		}

		if (line[2] == "LCSC Part")//Kicad Tools File over Kicad BOM export
		{
			valuCol = 1;
			fpCol = 0;
			lcscCol = 3;
			lcscBOM = true;
			continue;
		}

		QString value = line[valuCol].c_str();
		value = kicad_utils::CleanQuotes(value);

		if (value == "Value")
		{
			continue;
		}

		QString footp = line[fpCol].c_str();
		footp = kicad_utils::CleanQuotes(footp);

		if (std::any_of(partList.begin(), partList.end(), [&](auto const & elem)
			{ return elem.value == value && elem.footPrint == footp; })) {

			continue;
		}

		//if (partList)
		//{
		
		//}

		if (!lcscBOM)
		{
			digi = line[2].c_str();
			digi = kicad_utils::CleanQuotes(digi);
		}
		else
		{
			lcsc = line[2].c_str();
			lcsc = kicad_utils::CleanQuotes(lcsc);
		}
		partList.emplace_back(std::move(value), std::move(footp), std::move(digi), std::move(lcsc), std::move(mpn));
	}
	emit RedrawPartList(true);
}