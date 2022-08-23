#include "mainwindow.h"

#include "./ui_mainwindow.h"

#include "library_finder.h"
#include "schematic_adder.h"
#include "text_replace.h"

#include "addpartnumber.h"
#include "addmapping.h"

#include "mapping.h"

#include "kicad_utils.h"

#include "config.h"

#include <QMessageBox>
#include <QDesktopServices>
#include <QSettings>
#include <QFileDialog>
#include <QTextStream>
#include <QThread>
#include <QInputDialog>

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

	appdir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	std::filesystem::create_directory(appdir.toStdString());
	QString logdir = appdir + "/log/";
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

	settings = std::make_unique< QSettings>(appdir + "/settings.ini", QSettings::IniFormat);

	library_finder = std::make_unique<LibraryFinder>();
	connect( library_finder.get(), &LibraryFinder::SendMessage, this, &MainWindow::LogMessage );
	connect( library_finder.get(), &LibraryFinder::AddLibrary, this, &MainWindow::AddLibrary );
	connect( library_finder.get(), &LibraryFinder::SendResult, this, &MainWindow::AddFootPrintMsg );

	schematic_adder = std::make_unique<SchematicAdder>();
	connect( schematic_adder.get(), &SchematicAdder::SendMessage, this, &MainWindow::LogMessage );
	connect(schematic_adder.get(), &SchematicAdder::RedrawPartList, this, &MainWindow::RedrawPartList);
	connect(schematic_adder.get(), &SchematicAdder::UpdatePartRow, this, &MainWindow::UpdatePartRow);

	text_replace = std::make_unique<TextReplace>();
	connect(text_replace.get(), &TextReplace::SendMessage, this, &MainWindow::LogMessage);
	connect(text_replace.get(), &TextReplace::RedrawTextReplace, this, &MainWindow::RedrawMappingList);
	connect(text_replace.get(), &TextReplace::UpdateTextRow, this, &MainWindow::UpdateMappingRow);

	auto lastProject{ settings->value("last_project").toString() };

	if (QFile::exists(appdir + "/part_numbers.json"))
	{
		schematic_adder->LoadJsonFile(appdir + "/part_numbers.json");
	}
	if (QFile::exists(appdir + "/mapping.json"))
	{
		text_replace->LoadJsonFile(appdir + "/mapping.json");
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

void MainWindow::on_actionReload_Project_triggered() 
{
	SetProject(ui->leProject->text());
}

void MainWindow::on_actionImport_PartList_triggered()
{
	QString const partList = QFileDialog::getOpenFileName(this, "Select CSV PartNumbers File", settings->value("last_partnumbers").toString(), tr("CSV Files (*.csv);;All Files (*.*)"));
	if (!partList.isEmpty())
	{
		schematic_adder->ImportPartNumerCSV(partList);
		settings->setValue("last_partnumbers", partList);
		settings->sync();
	}
}

void MainWindow::on_actionImport_Rename_Map_triggered()
{
	QString const rename = QFileDialog::getOpenFileName(this, "Select CSV Rename File", settings->value("last_rename").toString(), tr("CSV Files (*.csv);;All Files (*.*)"));
	if (!rename.isEmpty())
	{
		text_replace->ImportMappingCSV(rename);
		settings->setValue("last_rename", rename);
		settings->sync();
	}
}

void MainWindow::on_actionOpen_Logs_triggered() 
{
	QDesktopServices::openUrl(QUrl::fromLocalFile(appdir + "/log/"));
}

void MainWindow::on_actionClose_triggered() 
{
	close();
}

void MainWindow::on_pbProjectFolder_clicked()
{
	QDesktopServices::openUrl(QUrl::fromLocalFile(ui->leProjectFolder->text()));
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

	std::vector<Mapping> replaceList;
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

void MainWindow::on_pbTextReplace_clicked()
{
	if(text_replace->getReplaceList().empty())
	{
		LogMessage("Replace List is empty", spdlog::level::level_enum::warn);
		return;
	}

	QStringList files ;

	for (int i = 0; i < ui->lwFiles->count(); ++i) {
		auto item = ui->lwFiles->item(i);
		if (item->checkState()){
			files.append(ui->leProjectFolder->text() + "/" + item->text());
		}
	}

	for(auto const& file : files )
	{
		if (!QFile::exists(file))
		{
			LogMessage("File Doesn't Exist", spdlog::level::level_enum::warn);
			continue;
		}
		ReplaceInFile(file, text_replace->getReplaceList());
		LogMessage(QString("Updating Text on %1 ").arg(file));
	}
}

void MainWindow::on_pbAddMap_clicked()
{
	AddMapping pn(this);
	if (pn.Load())
	{
		auto newMap{ pn.GetMapping() };
		text_replace->AddingMapping(newMap.first, newMap.second);
	}
}

void MainWindow::on_pbRemoveMap_clicked()
{
	QModelIndexList indexes = ui->lwWords->selectionModel()->selectedIndexes();

	QList<int> rowsList;

	for (int i = 0; i < indexes.count(); i++)
	{
		if (!rowsList.contains(indexes.at(i).row()))
		{
			rowsList.append(indexes.at(i).row());
		}
	}

	std::sort(rowsList.begin(), rowsList.end());

	for (int i = rowsList.count() - 1; i >= 0; i--)
	{
		text_replace->RemoveMapping(rowsList.at(i));
	}	
}

void MainWindow::on_pbReloadLibraries_clicked()
{
	QDir directory(ui->leProjectFolder->text());
	if (!directory.exists())
	{
		LogMessage("Directory Doesn't Exist", spdlog::level::level_enum::warn);
		return;
	}
	ClearLibrarys();
	library_finder->LoadProject(ui->leProjectFolder->text());
}

void MainWindow::on_pbCheckFP_clicked()
{
	ui->lwResults->clear();
	library_finder->CheckSchematics();
}

void MainWindow::on_pbSetPartsInSch_clicked() 
{
	schematic_adder->AddPartNumbersToSchematics(ui->leProjectFolder->text());
}

void MainWindow::on_pbAddPN_clicked()
{
	AddPartNumber pn(this);
	if (pn.Load())
	{
		auto newPart{ pn.GetPartInfo() };
		schematic_adder->AddPart(newPart);
	}
}

void MainWindow::on_pbRemovePN_clicked()
{
	//ui->twParts
	QModelIndexList indexes = ui->twParts->selectionModel()->selectedIndexes();

	QList<int> rowsList;

	for (int i = 0; i < indexes.count(); i++)
	{
		if (!rowsList.contains(indexes.at(i).row()))
		{
			rowsList.append(indexes.at(i).row());
		}
	}

	std::sort(rowsList.begin(), rowsList.end());

	for (int i = rowsList.count() - 1; i >= 0; i--)
	{
		schematic_adder->RemovePart(rowsList.at(i));
	}
}

void MainWindow::on_lwWords_cellDoubleClicked(int row, int column)
{
	auto header{ ui->lwWords->horizontalHeaderItem(column)->text() };
	auto value{ ui->lwWords->item(row, column)->text() };

	bool ok;
	QString text = QInputDialog::getText(this, header,
		header, QLineEdit::Normal,
		value, &ok);
	if (ok && !text.isEmpty())
	{
		ui->lwWords->item(row, column)->setText(text);
		text_replace->UpdateMapping(ui->lwWords->item(row, 0)->text(), ui->lwWords->item(row, 1)->text(),row);
	}	
}

void MainWindow::on_twParts_cellDoubleClicked(int row, int column)
{
	auto header{ ui->twParts->horizontalHeaderItem(column)->text() };
	auto value{ ui->twParts->item(row, column)->text() };

	bool ok;
	QString text = QInputDialog::getText(this, header,
		header, QLineEdit::Normal,
		value, &ok);
	if (ok && !text.isEmpty())
	{
		ui->twParts->item(row, column)->setText(text);
		schematic_adder->UpdatePart(ui->twParts->item(row, 0)->text(),
									ui->twParts->item(row, 1)->text(),
									ui->twParts->item(row, 2)->text(),
									ui->twParts->item(row, 3)->text(),
									ui->twParts->item(row, 4)->text(), row);
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
	auto const& kicadFiles = directory.entryInfoList(QStringList() << "*.kicad_sch" << "*.kicad_pcb", QDir::Files);
	for(auto const& file: kicadFiles)
	{
		ui->lwFiles->addItem(file.fileName());
	}

	QListWidgetItem* item = 0;
	for (int i = 0; i < ui->lwFiles->count(); ++i)
	{
		item = ui->lwFiles->item(i);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(Qt::Unchecked);
	}

	ClearLibrarys();
	library_finder->LoadProject(proj.absoluteDir().absolutePath());
}

void MainWindow::RedrawPartList(bool save)
{
	auto SetItem = [&](int row, int col, QString const& text)
	{
		ui->twParts->setItem(row, col, new QTableWidgetItem());
		ui->twParts->item(row, col)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		ui->twParts->item(row, col)->setText(text);
	};
	ui->twParts->clearContents();
	ui->twParts->setRowCount(schematic_adder->getPartList().size());
	int row{ 0 };

	for (auto const& line : schematic_adder->getPartList())
	{
		SetItem(row, 0, line.value);
		SetItem(row, 1, line.footPrint);
		SetItem(row, 2, line.digikey);
		SetItem(row, 3, line.lcsc);
		SetItem(row, 4, line.mpn);
		++row;
	}
	ui->twParts->resizeColumnsToContents();
	if (save)
	{
		schematic_adder->SaveJsonFile(appdir + "/part_numbers.json");
	}	
}

void MainWindow::RedrawMappingList(bool save)
{
	auto SetImportItem = [&](int row, int col, QString const& text)
	{
		ui->lwWords->setItem(row, col, new QTableWidgetItem());
		ui->lwWords->item(row, col)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		ui->lwWords->item(row, col)->setText(text);
	};
	ui->lwWords->clearContents();
	//ui->lwWords->clear();

	ui->lwWords->setRowCount(text_replace->getReplaceList().size());

	int rowIdx{ 0 };
	for (auto const& map : text_replace->getReplaceList())
	{
		SetImportItem(rowIdx, 0, map.from);
		SetImportItem(rowIdx, 1, map.to);
		++rowIdx;
	}

	ui->lwWords->resizeColumnsToContents();

	if (save)
	{
		text_replace->SaveJsonFile(appdir + "/mapping.json");
	}
}

void MainWindow::UpdatePartRow(int row)
{
	schematic_adder->SaveJsonFile(appdir + "/part_numbers.json");
}

void MainWindow::UpdateMappingRow(int row)
{
	text_replace->SaveJsonFile(appdir + "/mapping.json");
}

void MainWindow::ReplaceInFile(QString const& filePath, std::vector<Mapping> const& replaceList)
{
	try 
	{
		QStringList lines;
		QFile f(filePath);
		if (!f.open(QFile::ReadOnly | QFile::Text))
		{
			return;
		}
		QTextStream in(&f);

		while (!in.atEnd())
		{
			QString line = in.readLine();


			lines.append(line);
		}

		f.close();

		QStringList newlines;
		int lineNum{0};
		for (auto const& line : lines)
		{
			++lineNum;
			auto newline = line;
			for (auto const& mapp : replaceList)
			{
				newline.replace(QRegularExpression(mapp.from), mapp.to);
			}
			if(newline != line)
			{
				LogMessage( QString("%1 Update to '%2'").arg(lineNum).arg(newline));
			}
			newlines.append(newline);
		}

		QFile outFile(filePath);
		if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			return;
		}
		QTextStream out(&outFile);
		for (auto const& content : newlines)
		{
			out << content << "\n";
		}
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
	LogMessage(QString("Moving %1 : %2").arg(src.string().c_str()).arg(src.string().c_str()));
	if (createRoot)
	{
		std::filesystem::create_directory(target);
	}

	for (std::filesystem::path p : std::filesystem::directory_iterator(src))
	{
		std::filesystem::path destFile = target / p.filename();

		if (std::filesystem::is_directory(p)) 
		{
			std::filesystem::create_directory(destFile);
			MoveRecursive(p, destFile, false);
		}
		else		
		{
			std::filesystem::rename(p, destFile);
		}
	}
}

void MainWindow::AddLibrary( QString const& level, QString const& name, QString const& type, QString const& path)
{
	auto SetItem = [&](int row, int col, QString const& text)
	{
		ui->twLibraries->setItem(row, col, new QTableWidgetItem());
		ui->twLibraries->item(row, col)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		ui->twLibraries->item(row, col)->setText(text);
	};

	int row = ui->twLibraries->rowCount();
	ui->twLibraries->insertRow(row);

	SetItem(row, 0, level);
	SetItem(row, 1, name);
	SetItem(row, 2, type);
	SetItem(row, 3, path);

	ui->twLibraries->resizeColumnsToContents();
}

void MainWindow::ClearLibrarys()
{
	ui->twLibraries->clearContents();
	while( ui->twLibraries->rowCount() != 0 )
	{
		ui->twLibraries->removeRow(0);
	}
}

void MainWindow::AddFootPrintMsg( QString const& message, bool error)
{
	QListWidgetItem* it = new QListWidgetItem(message);
	if(error)
	{
		it->setForeground(QBrush(Qt::red));
		LogMessage(message, spdlog::level::level_enum::warn);
	}
	else
	{
		it->setForeground(QBrush(Qt::blue));
	}
	ui->lwResults->addItem(it);
	ui->lwResults->scrollToBottom();
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