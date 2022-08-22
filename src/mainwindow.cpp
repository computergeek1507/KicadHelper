#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "csvparser.h"

#include "library_finder.h"
#include "schematic_adder.h"

#include "config.h"

#include <QMessageBox>
#include <QDesktopServices>
#include <QSettings>
#include <QFileDialog>
#include <QTextStream>
#include <QThread>

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

	logger->info("Program started v" + std::string( PROJECT_VER));
	logger->info(std::string("Built with Qt ") + std::string(QT_VERSION_STR));

	setWindowTitle(windowTitle() + " v" + PROJECT_VER);

	settings = std::make_unique< QSettings>(dir + "settings.ini", QSettings::IniFormat);

	library_finder = std::make_unique<LibraryFinder>();
	connect( library_finder.get(), &LibraryFinder::SendMessage, this, &MainWindow::LogMessage );
	connect( library_finder.get(), &LibraryFinder::AddLibrary, this, &MainWindow::on_AddLibrary );

	schematic_adder = std::make_unique<SchematicAdder>();
	connect( schematic_adder.get(), &SchematicAdder::SendMessage, this, &MainWindow::LogMessage );

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
	QString const project = QFileDialog::getOpenFileName(this, "Select Kicad File", settings->value("last_project").toString(), tr("Kicad Files (*.kicad_pro *.pro);;All Files (*.*)"));
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
				LogMessage(QString("Copying %1 to %2").arg(file.fileName()).arg(newFile.fileName()));
				QFile::copy(file.filePath(), newName);
			}
			else
			{
				LogMessage(QString("Moving %1 to %2").arg(file.fileName()).arg(newFile.fileName()));
				QFile::rename(file.filePath(), newName);
			}
			QThread::msleep(100);
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
				LogMessage(QString("Copying %1 to %2").arg(diSource.string().c_str()).arg(diTarget.string().c_str()));
				CopyRecursive(diSource, diTarget);
			}
			else
			{
				LogMessage(QString("Moving %1 to %2").arg(diSource.string().c_str()).arg(diTarget.string().c_str()));
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
	QString const schematic = QFileDialog::getOpenFileName(this, "Select Kicad File", settings->value("last_project").toString(), tr("Kicad Files (*.kicad_pcb *.kicad_sch *.sch);;All Files (*.*)"));
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
	schematic_adder->AddPartNumbersToSchematics(ui->leProjectFolder->text());
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

	schematic_adder->ClearPartList();

	settings->setValue("last_partnumbers", csvFile);
	settings->sync();

	auto lines{ csvparser::parsefile(csvFile.toStdString()) };

	ui->twParts->setRowCount(lines.size());
	int row{ 0 };

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
			lcsc = CleanQuotes(lcsc);
			SetItem(row, 3, lcsc);
		}
		if (line.size() > 4)
		{
			mpn = line[4].c_str();
			mpn = CleanQuotes(mpn);
			SetItem(row, 4, mpn);
		}

		if(line[2] == "LCSC Part")//Kicad Tools File over Kicad BOM export
		{
			valuCol = 1;
			fpCol = 0;
			lcscCol = 3;
			lcscBOM = true;
			continue;
		}

		QString value = line[valuCol].c_str();
		value = CleanQuotes(value);

		if (value == "Value")
		{
			continue;
		}

		if(!lcscBOM)
		{
			digi = line[2].c_str();
			digi = CleanQuotes(digi);
			SetItem(row, 2, digi);
		}
		else
		{
			lcsc = line[2].c_str();
			lcsc = CleanQuotes(lcsc);
			SetItem(row, 3, lcsc);
		}

		QString footp = line[fpCol].c_str();
		footp = CleanQuotes(footp);
		
		SetItem(row, 0, value);
		SetItem(row, 1, footp);

		schematic_adder->AddPart(std::move(value),std::move( footp), std::move( digi), std::move(lcsc), std::move(mpn));

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

	for (fs::path p : fs::directory_iterator(src))
	{
		fs::path destFile = target / p.filename();

		if (fs::is_directory(p)) {
			fs::create_directory(destFile);
			MoveRecursive(p, destFile, false);
		}
		else 
		
		{
			fs::rename(p, destFile);

		}
	}
}

void MainWindow::UpdateSchematic(QString const& schPath)
{
	
}

void MainWindow::on_AddLibrary( QString const& name, QString const& type, QString const& path)
{

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