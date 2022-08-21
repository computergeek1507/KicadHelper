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
		QMessageBox::warning(this, "Error", "File Doesn't Exists");
		return;
	}
	if (ui->leOldName->text() == ui->leNewName->text())
	{
		QMessageBox::warning(this, "Error", "New and Old Names Are the same");
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
		return;
	}
	if (!QFile::exists(ui->leSchematic->text()))
	{
		return;
	}
	ReplaceInFile(ui->leSchematic->text(), replaceWordsList);
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
		return;
	}
	QDir directory(ui->leProjectFolder->text());
	if (!directory.exists())
	{
		return;
	}

	QStringList const& kicadFiles = directory.entryList(QStringList() << "*.kicad_sch" , QDir::Files);
	for (auto const& file : kicadFiles)
	{
		UpdateSchematic(file);
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
			ui->lePCB->setText(file);
			ui->leSchematic->setText(file);
			break;
		}
	}

	//QStringList const& csvFiles = directory.entryList(QStringList() << "*.csv", QDir::Files);
	//for (auto const& csv : csvFiles) {
	//	if (csv.contains("partnumbers") || csv.contains("part_numbers"))
	//	{
	//		ui->lePartsCSV->setText(csv);
	//		ImportPartNumerCSV(csv);
	//	}
	//	if (csv.contains("map") || csv.contains("rename"))
	//	{
	//		ui->leMapCSV->setText(csv);
	//		ImportRenameCSV(csv);
	//	}
	//}
}

void MainWindow::ImportRenameCSV(QString const& csvFile)
{
	ui->lwWords->clear();
	replaceWordsList.clear();
	ui->leMapCSV->setText(csvFile);
	settings->setValue("last_rename", csvFile);
	settings->sync();

	auto lines{ csvparser::parsefile(csvFile.toStdString()) };

	for (auto const& row : lines)
	{
		if (row.size() > 1)
		{
			ui->lwWords->addItem(QString("'%1' --> '%2'").arg(row[0].c_str()).arg(row[1].c_str()));
			replaceWordsList.emplace_back(row[0].c_str(), row[1].c_str());
		}
	}
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
			SetItem(row, 3, lcsc);
		}
		if (line.size() > 4)
		{
			mpn = line[4].c_str();
			SetItem(row, 4, mpn);
		}
		SetItem(row, 0, line[0].c_str());
		SetItem(row, 1, line[1].c_str());
		SetItem(row, 2, line[2].c_str());
		partList.emplace_back(line[0].c_str(), line[1].c_str(), line[2].c_str(), lcsc, mpn);

		++row;		
	}
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
			content.replace(QRegularExpression(Item1), Item2);
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
	QRegularExpression propRx(
		R"(\\(property\\s\\"(.*)\\"\s\\"(.*)\\"\s\\(id\\s(\\d +)\\)\\s\\(at\\s(-?\\d+.\\d+\\s-?\\d+.\\d+)\s\\d+\\")"
	);
	QRegularExpression pinRx (R"(\\(pin\\s\\"(.*)\\"\\s\\()");

	int lastID = 0;
	QString lastLoc = "";
	QString lastLcsc = "";
	QString newLcsc = "";

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

	QFile::rename(schPath, schPath + "_old");

	for (auto const& line : lines) {
		QString outLine = line;
		QRegularExpressionMatch m = propRx.match(line);

		if (m.hasMatch()) {
			auto key = m.captured(1);
			auto	value = m.captured(2);
			int lastID = m.captured(3).toInt();

			//found a LCSC property, so update it if needed
			if (key == "LCSC")
			{
				lastLcsc = value;
				if (lastLcsc == newLcsc)
				{
					outLine.replace(lastLcsc, newLcsc);
				}
			}
			if (key == "Reference")
			{
				lastLoc = m.captured(4);
				for (auto const& part : partList) {
					if (value != part.value)
					{
						newLcsc = part.lcsc;
						break;
					}
				}
			}
		}

//if we hit the pin section without finding a LCSC property, add it
		QRegularExpressionMatch pm = propRx.match(line);

		if (pm.hasMatch()) {

			if (lastLcsc == "" && newLcsc != "" && lastLoc != "" && lastID != 0)
			{
				QString newTxt = QString("    (property \"LCSC\" \"%1\" (id %2) (at %3 0)").arg(newLcsc).arg(lastID + 1).arg(lastLoc);
				newlines.push_back(newTxt);
				newlines.push_back("      (effects (font (size 1.27 1.27)) hide)");
				newlines.push_back("    )");
				lastID = 0;
				lastLoc = "";
				lastLcsc = "";
				newLcsc = "";
				newlines.push_back(outLine);
			}
		}			
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

	LogMessage(QString("Added LCSC's to %1(maybe?)").arg(schPath));
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