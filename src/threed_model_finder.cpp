#include "threed_model_finder.h"

#include "kicad_utils.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QFileInfo>
#include <QCoreApplication>

ThreeDModelFinder::ThreeDModelFinder()
{
}

void ThreeDModelFinder::LoadProject(QString const& folder)
{
	m_projectFolder = folder;
}

bool ThreeDModelFinder::CheckPCBs()
{
	QDir directory(m_projectFolder);
	if (!directory.exists())
	{
		emit SendMessage("Directory Doesn't Exist", spdlog::level::level_enum::warn, QString());
		return false;
	}

	incorrectThreeDModelFileList.clear();

	auto const& kicadFiles{ directory.entryInfoList(QStringList() << "*.kicad_pcb" , QDir::Files) };
	for (auto const& file : kicadFiles)
	{
		emit SendMessage(QString("Checking '%1'").arg(file.fileName()), spdlog::level::level_enum::debug, file.absoluteFilePath());
		CheckPCB(file.absoluteFilePath());
	}
	return true;
}

void ThreeDModelFinder::CheckPCB(QString const& pcbPath)
{
	QFileInfo fileData(pcbPath);
	QFile inFile(pcbPath);

	if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		emit SendMessage(QString("Could not Open '%1'").arg(pcbPath), spdlog::level::level_enum::warn, pcbPath);
		return;
	}
	bool errorFound{ false };
	//bool partSection{false};
	//int lineCount{0};

	QString reference;
	QTextStream in(&inFile);
	while (!in.atEnd())
	{
		//lineCount++;
		QString const line = in.readLine();

		QString const ref = getPCBReference(line);
		if (!ref.isEmpty())
		{
			reference = ref;
		}

		QString const model = getThreeDModelPath(line);
		if (model.isEmpty())
		{
			continue;
		}
		if (!model.startsWith("$"))
		{
			if (!incorrectThreeDModelFileList.contains(pcbPath))
			{
				incorrectThreeDModelFileList.append(pcbPath);
			}
			emit SendResult(QString("'%1':'%2' has incorrect path in '%3'").arg(reference).arg(model).arg(fileData.fileName()), true);
			errorFound = true;
		}
		//reference.clear();
	}
	inFile.close();

	if (!errorFound)
	{
		emit SendResult(QString("'%1' is Good").arg(fileData.fileName()), false);
	}
}


bool ThreeDModelFinder::FixThreeDModels(QString const& folder)
{
	bool worked{ false };
	/*
		Three Steps of "fixing" footprints
		1) Convert absolute paths to relative paths in Footprint Table
		2) Find librarys in library folder and add Footprint Table
		3) IDK
	*/
	if (incorrectThreeDModelFileList.empty())
	{
		CheckPCBs();
	}

	//if(ConvertAllPathsToRelative(folder))
	//{
	//    emit SendClearResults();
	//    CheckPCBs();
	//}

	for (auto const& filePath : incorrectThreeDModelFileList)
	{
		if (AttemptToFixThreeDModelFile(filePath, folder))
		{
			worked = true;
			emit SendMessage(QString("Fixed '%1'").arg(filePath), spdlog::level::level_enum::debug, filePath);
		}
	}
	if (worked)
	{
		//QString localLibPath {m_projectFolder + "/fp-lib-table"};
		emit SendClearResults();
		CheckPCBs();
	}

	return worked;
}

bool ThreeDModelFinder::AttemptToFixThreeDModelFile(QString const& pcbPath, QString const& libraryPath)
{
	std::vector<QString> lines;
	std::vector<QString> newlines;

	QFile inFile(pcbPath);

	if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		emit SendMessage(QString("Could not Open '%1'").arg(pcbPath), spdlog::level::level_enum::warn, pcbPath);
		return false;
	}
	QTextStream in(&inFile);
	while (!in.atEnd())
	{
		lines.emplace_back(in.readLine());
	}
	inFile.close();
	std::map<QString, QString> convertList;
	QString reference;

	for (auto const& line : lines)
	{
		QString outLine = line;

		QString const ref = getPCBReference(line);
		if (!ref.isEmpty())
		{
			reference = ref;
		}

		QString const model = getThreeDModelPath(line);

		if (!model.isEmpty() && !model.startsWith("$"))
		{
			if (convertList.contains(model))
			{
				outLine = outLine.replace(model, convertList.at(model));
				emit SendMessage(QString("Updating '%1' to '%2'").arg(model).arg(convertList.at(model)), spdlog::level::level_enum::debug, QString());
			}
			else
			{
				auto const newPath = ConvertToRelativePath(model, libraryPath);
				if (!newPath.isEmpty() && newPath != model)
				{
					convertList.insert({ model, newPath });
					outLine = outLine.replace(model, newPath);
					emit SendMessage(QString("Updating '%1' to '%2'").arg(model).arg(newPath), spdlog::level::level_enum::debug, QString());
				}
			}
		}
		newlines.push_back(outLine);
	}

	try
	{
		if (QFile::exists(pcbPath + "_old"))
		{
			QFile::remove(pcbPath + "_old");
		}

		QFile::rename(pcbPath, pcbPath + "_old");
	}
	catch (std::exception /*ex*/)
	{
		emit SendMessage(QString("Could not Create '%1_old'").arg(pcbPath), spdlog::level::level_enum::warn, pcbPath);
	}

	QFile outFile(pcbPath);
	if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		emit SendMessage(QString("Could not Open '%1'").arg(pcbPath), spdlog::level::level_enum::warn, pcbPath);
		return false;
	}
	QTextStream out(&outFile);
	for (auto const& line : newlines) {
		out << line << "\n";
	}
	outFile.close();

	return true;
}

QString ThreeDModelFinder::getThreeDModelPath(QString const& line) const
{
	//    (model "${KICAD6_3DMODEL_DIR}/Resistor_SMD.3dshapes/R_0603_1608Metric.wrl"
	QString prop(R"((model )");
	int stInd = line.indexOf(prop);

	if (stInd == -1)
	{
		return QString();
	}
	int endInd = line.indexOf("(", stInd + 1);

	if (endInd == -1)
	{
		endInd = line.length();
	}

	if (stInd == endInd || stInd == -1 || endInd == -1)
	{
		return QString();
	}
	int nstr{ stInd + prop.size() + 1 };
	QString value = line.mid(nstr, endInd - nstr - 1);

	value = kicad_utils::CleanQuotes(value);
	return value;
}


QString ThreeDModelFinder::getPCBReference(QString const& line) const
{
	//    (fp_text reference "J30" 
	QString prop(R"((fp_text reference)");
	int stInd = line.indexOf(prop);

	if (stInd == -1)
	{
		return QString();
	}
	int endInd = line.indexOf("(", stInd + 1);

	if (stInd == endInd || stInd == -1 || endInd == -1)
	{
		return QString();
	}
	int nstr{ stInd + prop.size() + 1 };
	QString value = line.mid(nstr, endInd - nstr - 1);

	value = kicad_utils::CleanQuotes(value);
	return value;
}
