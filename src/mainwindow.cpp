#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "csvparser.h"

#include <QMessageBox>
#include <QDesktopServices>
#include <QSettings>
#include <QFileDialog>
#include <QTextStream>
#include <QThread>
#include <QRegularExpression>

#include "spdlog/spdlog.h"

#include "spdlog/sinks/qt_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"

#include <filesystem>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

	auto const log_name{ "log.txt" };

	QString const dir = QCoreApplication::applicationDirPath();
	std::filesystem::create_directory(dir.toStdString());
	QString logdir = dir + "/log/";
	std::filesystem::create_directory(logdir.toStdString());

	try
	{
		auto file{ std::string(logdir.toStdString() + log_name) };
		auto rotating = std::make_shared<spdlog::sinks::rotating_file_sink_mt>( file, 1024 * 1024, 5, false);
		//rotating->set_level(spdlog::level::debug);
		//rotating->flush
		
		logger = std::make_shared<spdlog::logger>("kicadhelper", rotating);
		logger->flush_on(spdlog::level::debug);
		logger->set_level(spdlog::level::debug);

		//spdlog::logger logger("multi_sink", { console_sink, file_sink });
	}
	catch (std::exception& /*ex*/)
	{
		QMessageBox::warning(this, "Logger Failed", "Logger Failed To Start.");
	}

	logger->info("Program started");
	logger->info(std::string("Built with Qt ") + std::string(QT_VERSION_STR));

	settings = std::make_unique< QSettings>(dir + "settings.ini", QSettings::IniFormat);

	auto lastProject{ settings->value("last_project").toString() };
	auto lastRename{ settings->value("last_rename").toString() };
	auto lastPartList{ settings->value("last_partnumbers").toString() };

	if (QFile::exists(lastRename))
	{
		ImportRenameCSV(lastRename);
	}
	if (QFile::exists(lastPartList))
	{
		ImportPartNumerCSV(lastPartList);
	}
	if (QFile::exists(lastProject))
	{
		SetProject(lastProject);
	}
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionOpen_Project_triggered() 
{
	//Kicad files (*.pro;*.kicad_pro)|*.pro;*.kicad_pro|All Files|*.*
	QString const project = QFileDialog::getOpenFileName(this, "Select Kicad File", settings->value("last_project").toString(), tr("Kicad Files (*.pro;*.kicad_pro);;All Files (*.*)"));
	if (project.isEmpty())
	{
		return;
	}
	SetProject(project);
}

void MainWindow::on_actionOpen_Logs_triggered() 
{
	QDesktopServices::openUrl(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/log/"));
}

void MainWindow::on_actionClose_triggered() 
{
	close();
}

void MainWindow::on_pbProject_clicked() 
{
	on_actionOpen_Project_triggered();
}

void MainWindow::on_pbRename_clicked()
{
	QFileInfo project(ui->leProject->text());
	if (!project.exists())
	{
		LogMessage("File Doesn't Exist", spdlog::level::level_enum::warn);
		return;
	}
	if (ui->leOldName->text() == ui->leNewName->text())
	{
		LogMessage("New and Old Names Are the same", spdlog::level::level_enum::warn);
		return;
	}

	std::vector<std::pair<QString, QString>> replaceList;
	replaceList.emplace_back(ui->leOldName->text() , ui->leNewName->text());

	bool const copy = ui->cbCopyFiles->isChecked();

	for (const auto& file : project.dir().entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden | QDir::Files ))
	{
		//QString const test = partVerFileInfoList[i].filePath();
		QString newName = file.filePath().replace(ui->leOldName->text(), ui->leNewName->text());
		if (file.filePath() == newName)
		{
			continue;
		}
		try
		{
			QFileInfo newFile(newName);

			if (!newFile.dir().exists())
			{
				std::filesystem::create_directory(newFile.dir().absolutePath().toStdString());
				//Directory.CreateDirectory(path);
			}

			if (copy)
			{
				QFile::copy(file.filePath(), newName);
			}
			else
			{
				QFile::rename(file.filePath(), newName);
			}
			QThread::sleep(100);
			ReplaceInFile(newName, replaceList);
		}
		catch (std::exception ex)
		{
			LogMessage(ex.what(), spdlog::level::err);
		}
	}

	for (const auto& folder : project.dir().entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden | QDir::AllDirs))
	{
		try
		{
			// folderName  is the folder name
			QString newfolder = folder.filePath().replace(ui->leOldName->text(), ui->leNewName->text());
			if (folder.filePath() == newfolder)
			{
				continue;
			}

			std::filesystem::path diSource(folder.filePath().toStdString()) ;
			std::filesystem::path diTarget(newfolder.toStdString());

			if (copy)
			{
				CopyRecursive(diSource, diTarget);
			}
			else
			{
				MoveRecursive(diSource, diTarget);
			}

		}
		catch (std::exception ex)
		{
			LogMessage(ex.what(), spdlog::level::err);
		}
	}
}

void MainWindow::on_pbFindSchematic_clicked()
{
	QString const schematic = QFileDialog::getOpenFileName(this, "Select Kicad File", settings->value("last_project").toString(), tr("Kicad Files (*.kicad_pcb;*.kicad_sch;*.sch);;All Files (*.*)"));
	if (!schematic.isEmpty())
	{
		ui->leSchematic->setText(schematic);
	}
}

void MainWindow::on_pbImportCSV_clicked()
{
	QString const rename = QFileDialog::getOpenFileName(this, "Select CSV Rename File", settings->value("last_rename").toString(), tr("CSV Files (*.csv);;All Files (*.*)"));
	if (!rename.isEmpty())
	{
		ImportRenameCSV(rename);
	}
}

void MainWindow::on_pbTextReplace_clicked()
{
	if(replaceWordsList.empty())
	{
		LogMessage("Replace List is empty", spdlog::level::level_enum::warn);
		return;
	}
	if (!QFile::exists(ui->leSchematic->text()))
	{
		LogMessage("File Doesn't Exist", spdlog::level::level_enum::warn);
		return;
	}
	ReplaceInFile(ui->leSchematic->text(), replaceWordsList);
	LogMessage(QString("Updating Text on %1 ").arg(ui->leSchematic->text()));
}

void MainWindow::on_pbFindPCB_clicked()
{
	QString const pcbs = QFileDialog::getOpenFileName(this, "Select Kicad PCB File", settings->value("last_project").toString(), tr("Kicad PCB Files (*.kicad_pcb);;All Files (*.*)"));
	if (!pcbs.isEmpty())
	{
		ui->lePCB->setText(pcbs);
	}
}

void MainWindow::on_pbCheckFP_clicked()
{
	if (!QFile::exists(ui->lePCB->text()))
	{
		LogMessage("File Doesn't Exist", spdlog::level::level_enum::warn);
		return;
	}
	//todo
}

void MainWindow::on_pbFindProjet_clicked()
{
	QString directory = QFileDialog::getExistingDirectory(this, "Set Kicad Project Directory", "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	if (!directory.isEmpty())
	{
		ui->leProjectFolder->setText(directory);
	}	
}

void MainWindow::on_pbImportParts_clicked()
{
	QString const partList = QFileDialog::getOpenFileName(this, "Select CSV PartNumbers File", settings->value("last_partnumbers").toString(), tr("CSV Files (*.csv);;All Files (*.*)"));
	if (!partList.isEmpty())
	{
		ImportPartNumerCSV(partList);		
	}
}

void MainWindow::on_pbSetPartsInSch_clicked() 
{
	if (partList.empty())
	{
		LogMessage("Part List is empty", spdlog::level::level_enum::warn);
		return;
	}
	QDir directory(ui->leProjectFolder->text());
	if (!directory.exists())
	{
		LogMessage("Directory Doesn't Exist", spdlog::level::level_enum::warn);
		return;
	}

	QStringList const& kicadFiles = directory.entryList(QStringList() << "*.kicad_sch" , QDir::Files);
	for (auto const& file : kicadFiles)
	{
		LogMessage(QString("Updating PN's in %1").arg(file));
		UpdateSchematic(directory.absolutePath() + "/" + file);
	}
}

void MainWindow::SetProject(QString const& project)
{
	ui->leProject->setText(project);
	settings->setValue("last_project", project);
	settings->sync();

	QFileInfo proj(project);

	ui->leProjectFolder->setText(proj.absoluteDir().absolutePath());

	ui->leOldName->setText(proj.baseName());
	ui->leNewName->setText(proj.baseName());

	QDir directory(proj.absoluteDir().absolutePath());
	QStringList const& kicadFiles = directory.entryList(QStringList() << "*.kicad_sch" << "*.kicad_pcb", QDir::Files);
	for(auto const& file: kicadFiles) {
		if (file.endsWith(".kicad_pcb"))
		{
			ui->lePCB->setText(directory.absolutePath() + "/" + file);
			ui->leSchematic->setText(directory.absolutePath() + "/" + file);
			break;
		}
	}

	//QStringList const& csvFiles = directory.entryList(QStringList() << "*.csv", QDir::Files);
	//for (auto const& csv : csvFiles) {
	//	if (csv.contains("partnumbers") || csv.contains("part_numbers"))
	//	{
	//		ui->lePartsCSV->setText(csv);
	//		ImportPartNumerCSV(directory.absolutePath() + "/" + csv);
	//	}
	//	if (csv.contains("map") || csv.contains("rename"))
	//	{
	//		ui->leMapCSV->setText(csv);
	//		ImportRenameCSV(directory.absolutePath() + "/" + csv);
	//	}
	//}
}

void MainWindow::ImportRenameCSV(QString const& csvFile)
{
	auto SetImportItem = [&](int row, int col, QString const& text)
	{
		ui->lwWords->setItem(row, col, new QTableWidgetItem());
		ui->lwWords->item(row, col)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		ui->lwWords->item(row, col)->setText(text);
	};
	ui->lwWords->clearContents();
	ui->lwWords->clear();
	replaceWordsList.clear();
	ui->leMapCSV->setText(csvFile);
	settings->setValue("last_rename", csvFile);
	settings->sync();

	auto lines{ csvparser::parsefile(csvFile.toStdString()) };

	ui->lwWords->setRowCount(lines.size());

	int rowIdx{ 0 };
	for (auto const& row : lines)
	{
		if (row.size() > 1)
		{
			SetImportItem(rowIdx, 0, CleanQuotes(row[0].c_str()));
			SetImportItem(rowIdx, 1, CleanQuotes(row[1].c_str()));
			replaceWordsList.emplace_back(row[0].c_str(), row[1].c_str());
			++rowIdx;
		}
	}

	ui->lwWords->resizeColumnsToContents();
}

void MainWindow::ImportPartNumerCSV(QString const& csvFile)
{
	auto SetItem = [&](int row, int col, QString const& text)
	{
		ui->twParts->setItem(row, col, new QTableWidgetItem());
		ui->twParts->item(row, col)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		ui->twParts->item(row, col)->setText(text);
	};
	ui->twParts->clearContents();
	ui->lePartsCSV->setText(csvFile);
	settings->setValue("last_partnumbers", csvFile);
	settings->sync();

	auto lines{ csvparser::parsefile(csvFile.toStdString()) };

	ui->twParts->setRowCount(lines.size());
	int row{ 0 };
	for (auto const& line : lines)
	{				
		if (line.size() < 3)
		{
			continue;
		}
		QString lcsc;
		QString mpn;
		if (line.size() > 3)
		{
			lcsc = line[3].c_str();
			lcsc = CleanQuotes(lcsc);
			SetItem(row, 3, lcsc);
		}
		if (line.size() > 4)
		{
			mpn = line[4].c_str();
			mpn = CleanQuotes(mpn);
			SetItem(row, 4, mpn);
		}
		QString value = line[0].c_str();
		value = CleanQuotes(value);

		if (value == "Value")
		{
			continue;
		}

		QString footp = line[1].c_str();
		footp = CleanQuotes(footp);
		QString digi = line[2].c_str();
		digi = CleanQuotes(digi);
		SetItem(row, 0, value);
		SetItem(row, 1, footp);
		SetItem(row, 2, digi);
		partList.emplace_back(value, footp, digi, lcsc, mpn);

		++row;		
	}
	ui->twParts->resizeColumnsToContents();
}

void MainWindow::ReplaceInFile(QString const& filePath, std::vector<std::pair<QString, QString>> const& replaceList)
{
	try 
	{
		QFile f(filePath);
		if (!f.open(QFile::ReadOnly | QFile::Text))
		{
			return;
		}
		QTextStream in(&f);
		QString content{ in.readAll() };
		f.close();

		for (auto const& [Item1, Item2] : replaceList)
		{
			content = content.replace(QRegularExpression(Item1), Item2);
		}

		QFile outFile(filePath);
		if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			return;
		}
		QTextStream out(&outFile);
		out << content;
		outFile.close();
	}
	catch (std::exception& ex)
	{
		LogMessage( ex.what(), spdlog::level::err);
	}
	catch (...)
	{

	}
}
// Recursively copies all files and folders from src to target and overwrites existing files in target.
void MainWindow::CopyRecursive(const std::filesystem::path& src, const std::filesystem::path& target)
{
	try
	{
		LogMessage(QString("Coping %1 : %2").arg(src.string().c_str()).arg(src.string().c_str()));
		std::filesystem::copy(src, target, std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive);
	}
	catch (std::exception& e)
	{
		LogMessage(e.what(), spdlog::level::err);
	}
}

void MainWindow::MoveRecursive(const std::filesystem::path& src, const std::filesystem::path& target,  bool createRoot)
{
	namespace fs = std::filesystem;
	LogMessage(QString("Moving %1 : %2").arg(src.string().c_str()).arg(src.string().c_str()));
	if (createRoot)
	{
		fs::create_directory(target);
	}

	for (fs::path p : fs::directory_iterator(src)) {
		fs::path destFile = target / p.filename();

		if (fs::is_directory(p)) {
			fs::create_directory(destFile);
			MoveRecursive(p, destFile, false);
		}
		else {
			//std::cout << "updated " << p.string().c_str() << std::endl;
			fs::rename(p, destFile);

		}
	}
}

void MainWindow::UpdateSchematic(QString const& schPath)
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

	std::vector<QString> lines;
	std::vector<QString> newlines;

	QFile inFile(schPath);

	if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		return;
	}
	QTextStream in(&inFile);
	while (!in.atEnd())
	{
		lines.emplace_back(in.readLine());
	}
	inFile.close();

	for (auto const& line : lines)
	{
		QString outLine = line;
		QRegularExpressionMatch m = propRx.match(line);

		if (m.hasMatch() && line.startsWith("    (property"))
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
					LogMessage(QString("Updating %1 Digikey to %2").arg(lastRef).arg(newDigikey));
					outLine.replace("\"" + lastDigikey + "\"", "\"" + newDigikey + "\"");
					lastDigikey = newDigikey;
				}
			}

			//found a LCSC property, so update it if needed
			if (key == "LCSC")
			{
				lastLcsc = value;
				if (lastLcsc != newLcsc && !newLcsc.isEmpty())
				{
					LogMessage(QString("Updating %1 LCSC to %2").arg(lastRef).arg(newLcsc));
					outLine.replace("\"" + lastLcsc + "\"", "\"" + newLcsc + "\"");
					lastLcsc = newLcsc;
				}
			}
			if (key == "MPN")
			{
				lastMPN = value;
				if (lastMPN != newMPN && !newMPN.isEmpty())
				{
					LogMessage(QString("Updating %1 MPN to %2").arg(lastRef).arg(newMPN));
					outLine.replace("\"" + lastMPN + "\"", "\"" + newMPN + "\"");
					lastMPN = newMPN;
				}
			}

			if (key == "Reference")
			{
				lastLoc = m.captured(4);
				lastRef = value;
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
			if (key == "Value")
			{
				for (auto const& part : partList) {
					if (value == part.value)
					{
						newDigikey = part.digikey;
						newLcsc = part.lcsc;
						newMPN = part.mpn;
						break;
					}
				}
			}
		}

//if we hit the pin section without finding a LCSC property, add it
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
					LogMessage(QString("Adding %1 DigiKey %2").arg(lastRef).arg(newDigikey));
					QString newTxt = QString("    (property \"Digi-Key_PN\" \"%1\" (id %2) (at %3 0)").arg(newDigikey).arg(newid).arg(lastLoc);
					newlines.push_back(newTxt);
					newlines.push_back("      (effects (font (size 1.27 1.27)) hide)");
					newlines.push_back("    )");
					newid++;
				}
				if (addLcsc)
				{
					LogMessage(QString("Adding %1 LCSC %2").arg(lastRef).arg(newLcsc));
					QString newTxt = QString("    (property \"LCSC\" \"%1\" (id %2) (at %3 0)").arg(newLcsc).arg(newid).arg(lastLoc);
					newlines.push_back(newTxt);
					newlines.push_back("      (effects (font (size 1.27 1.27)) hide)");
					newlines.push_back("    )");
					newid++;
				}
				if (addMPN)
				{
					LogMessage(QString("Adding %1 MPN %2").arg(lastRef).arg(newMPN));
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
	}

	QFile outFile(schPath);
	if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		return;
	}
	QTextStream out(&outFile);
	for (auto const& line : newlines) {
		out << line << "\n";
	}	
	outFile.close();
}

QString MainWindow::CleanQuotes(QString item) const
{
	//item = item.trimmed();
	if (item.startsWith("\""))
	{
		item.remove(0, 1);
	}
	if (item.endsWith("\""))
	{
		int pos = item.lastIndexOf("\"");
		item = item.left(pos);
	}
	return item;
}

void MainWindow::LogMessage(QString const& message, spdlog::level::level_enum llvl)
{
	logger->log(llvl, message.toStdString());
	/*
	trace = SPDLOG_LEVEL_TRACE,
	debug = SPDLOG_LEVEL_DEBUG,
	info = SPDLOG_LEVEL_INFO,
	warn = SPDLOG_LEVEL_WARN,
	err = SPDLOG_LEVEL_ERROR,
	critical = SPDLOG_LEVEL_CRITICAL,
	*/

	QList< QColor > msgColors;
	msgColors.append(Qt::darkBlue);
	msgColors.append(Qt::blue);
	msgColors.append(Qt::darkGreen);
	msgColors.append(Qt::darkYellow);
	msgColors.append(Qt::darkRed);
	msgColors.append(Qt::red);

	QListWidgetItem* it = new QListWidgetItem(message);
	it->setForeground(QBrush(QColor(msgColors.at(llvl))));
	ui->lwLogs->addItem(it);
	ui->lwLogs->scrollToBottom();

	qApp->processEvents();

}