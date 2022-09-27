#include "mainwindow.h"

#include "./ui_mainwindow.h"

#include "footprint_finder.h"
#include "symbol_finder.h" 
#include "threed_model_finder.h" 
#include "schematic_adder.h"
#include "text_replace.h"

#include "addpartnumber.h"
#include "addmapping.h"
#include "viewlist.h"

#include "mapping.h"

#include "kicad_utils.h"

#include "config.h"

#include <QMessageBox>
#include <QDesktopServices>
#include <QSettings>
#include <QFileDialog>
#include <QTextStream>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTableWidget>
#include <QThread>
#include <QInputDialog>
#include <QCommandLineParser>
#include <QTimer>

#include "spdlog/spdlog.h"

#include "spdlog/sinks/qt_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"

#include <filesystem>

enum LibraryColumns { Name, Type, URL, Descr };

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
	QCoreApplication::setApplicationName(PROJECT_NAME);
    QCoreApplication::setApplicationVersion(PROJECT_VER);
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

		logger = std::make_shared<spdlog::logger>("kicadhelper", rotating);
		logger->flush_on(spdlog::level::debug);
		logger->set_level(spdlog::level::debug);
		logger->set_pattern("[%D %H:%M:%S] [%L] %v");
	}
	catch (std::exception& /*ex*/)
	{
		QMessageBox::warning(this, "Logger Failed", "Logger Failed To Start.");
	}

	//logger->info("Program started v" + std::string( PROJECT_VER));
	//logger->info(std::string("Built with Qt ") + std::string(QT_VERSION_STR));

	setWindowTitle(windowTitle() + " v" + PROJECT_VER);

	settings = std::make_unique< QSettings>(appdir + "/settings.ini", QSettings::IniFormat);

	footprint_finder = std::make_unique<FootprintFinder>();
	connect(footprint_finder.get(), &LibraryBase::SendMessage, this, &MainWindow::LogMessage );
	connect(footprint_finder.get(), &LibraryBase::SendAddLibrary, this, &MainWindow::AddFootprintLibrary );
	connect(footprint_finder.get(), &LibraryBase::SendClearLibrary, this, &MainWindow::ClearFootprintLibrary );
	connect(footprint_finder.get(), &LibraryBase::SendUpdateLibraryRow, this, &MainWindow::UpdateFootprintLibraryRow);
	connect(footprint_finder.get(), &LibraryBase::SendResult, this, &MainWindow::AddFootprintMsg );
	connect(footprint_finder.get(), &LibraryBase::SendClearResults, this, &MainWindow::ClearFootprintMsgs );
	connect(footprint_finder.get(), &LibraryBase::SendLibraryError, this, &MainWindow::SetSymbolLibraryError);

	symbol_finder = std::make_unique<SymbolFinder>();
	connect(symbol_finder.get(), &LibraryBase::SendMessage, this, &MainWindow::LogMessage );
	connect(symbol_finder.get(), &LibraryBase::SendAddLibrary, this, &MainWindow::AddSymbolLibrary );
	connect(symbol_finder.get(), &LibraryBase::SendClearLibrary, this, &MainWindow::ClearSymbolLibrary );
	connect(symbol_finder.get(), &LibraryBase::SendUpdateLibraryRow, this, &MainWindow::UpdateSymbolLibraryRow);
	connect(symbol_finder.get(), &LibraryBase::SendResult, this, &MainWindow::AddSymbolMsg );
	connect(symbol_finder.get(), &LibraryBase::SendClearResults, this, &MainWindow::ClearSymbolMsgs );
	connect(symbol_finder.get(), &LibraryBase::SendLibraryError, this, &MainWindow::SetSymbolLibraryError);

	threed_model_finder = std::make_unique<ThreeDModelFinder>();
	connect(threed_model_finder.get(), &LibraryBase::SendMessage, this, &MainWindow::LogMessage);
	connect(threed_model_finder.get(), &LibraryBase::SendResult, this, &MainWindow::AddThreeDModelMsg);
	connect(threed_model_finder.get(), &LibraryBase::SendClearResults, this, &MainWindow::ClearThreeDModelMsgs);

	schematic_adder = std::make_unique<SchematicAdder>();
	connect(schematic_adder.get(), &SchematicAdder::SendMessage, this, &MainWindow::LogMessage );
	connect(schematic_adder.get(), &SchematicAdder::RedrawPartList, this, &MainWindow::RedrawPartList);
	connect(schematic_adder.get(), &SchematicAdder::UpdatePartRow, this, &MainWindow::UpdatePartRow);

	text_replace = std::make_unique<TextReplace>();
	connect(text_replace.get(), &TextReplace::SendMessage, this, &MainWindow::LogMessage);
	connect(text_replace.get(), &TextReplace::RedrawTextReplace, this, &MainWindow::RedrawMappingList);
	connect(text_replace.get(), &TextReplace::UpdateTextRow, this, &MainWindow::UpdateMappingRow);

	bool overrideImport = settings->value("override", false).toBool();

	ui->actionOverride->setChecked(overrideImport);

	RedrawRecentList();

	int index = settings->value("table_index", -1).toInt();
	if (-1 != index)
	{
		ui->tabWidget->setCurrentIndex(index);
	}

	QTimer::singleShot( 500, this, SLOT( ProcessCommandLine() ) );
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionOpen_Project_triggered()
{
	//Kicad files (*.pro;*.kicad_pro)|*.pro;*.kicad_pro|All Files|*.*
	QString const project = QFileDialog::getOpenFileName(this, "Select Kicad File", settings->value("last_project").toString(), tr("Kicad Files (*.kicad_pro *.pro);;All Files (*.*)"));
	if (!project.isEmpty())
	{
		SetProject(project);
	}
}

void MainWindow::on_actionReload_Project_triggered()
{
	SetProject(ui->leProject->text());
}

void MainWindow::on_actionSet_Library_Folder_triggered() 
{
	QString const library = QFileDialog::getExistingDirectory(this, "Select Kicad File", settings->value("last_project").toString());
	if (!library.isEmpty())
	{
		SetLibrary(library);
	}
}

void MainWindow::on_actionImport_Rename_Map_triggered()
{
	QString const rename = QFileDialog::getOpenFileName(this, "Select Rename File", settings->value("last_rename").toString(), tr("CSV Files (*.csv);;JSON Files (*.json);;All Files (*.*)"));
	if (!rename.isEmpty())
	{
		if(rename.endsWith("json", Qt::CaseInsensitive))
		{
			text_replace->LoadJsonFile(rename);
		}
		else
		{
			text_replace->ImportMappingCSV(rename);
			settings->setValue("last_rename", rename);
			settings->sync();
		}
	}
}

void MainWindow::on_actionImport_PartList_triggered()
{
	QString const partList = QFileDialog::getOpenFileName(this, "Select PartNumbers File", settings->value("last_partnumbers").toString(), tr("CSV Files (*.csv);;JSON Files (*.json);;All Files (*.*)"));
	if (!partList.isEmpty())
	{	
		if(partList.endsWith("json", Qt::CaseInsensitive))
		{
			schematic_adder->LoadJsonFile(partList);
		}
		else
		{
			schematic_adder->ImportPartNumerCSV(partList, ui->actionOverride->isChecked());
			settings->setValue("last_partnumbers", partList);
			settings->sync();
		}
	}
}

void MainWindow::on_actionImport_Parts_from_Schematic_triggered()
{
	QStringList const schList = QFileDialog::getOpenFileNames(this, "Select Kicad Schematic Files",ui->leProjectFolder->text(), tr("Kicad Schematic Files (*.kicad_sch);;All Files (*.*)"));
	if (!schList.isEmpty())
	{
		schematic_adder->ImportSchematicParts(schList,ui->actionOverride->isChecked());
	}
}

void MainWindow::on_actionOverride_triggered()
{
	settings->setValue("override", ui->actionOverride->isChecked());
	settings->sync();
}

void MainWindow::on_actionExport_Rename_CSV_triggered()
{
	QString const rename = QFileDialog::getSaveFileName(this, "Save Rename File", settings->value("last_rename").toString(), tr("CSV Files (*.csv);;JSON Files (*.json);;All Files (*.*)"));
	if (!rename.isEmpty())
	{
		if(rename.endsWith("json", Qt::CaseInsensitive))
		{
			text_replace->SaveJsonFile(rename);
		}
		else
		{
			text_replace->SaveMappingCSV(rename);
		}
	}
}

void MainWindow::on_actionExport_PartList_CSV_triggered()
{
	QString const partList = QFileDialog::getSaveFileName(this, "Save PartNumbers File", settings->value("last_partnumbers").toString(), tr("CSV Files (*.csv);;JSON Files (*.json);;All Files (*.*)"));
	if (!partList.isEmpty())
	{
		if(partList.endsWith("json", Qt::CaseInsensitive))
		{
			schematic_adder->SaveJsonFile(partList);
		}
		else
		{
			schematic_adder->SavePartNumerCSV(partList);
		}
	}
}

void MainWindow::on_actionLibrary_Report_triggered()
{
	QString const reportFile = QFileDialog::getSaveFileName(this, "Save Report File", ui->leProjectFolder->text()+ "/report.txt", tr("txt Files (*.txt);;All Files (*.*)"));
	if (!reportFile.isEmpty())
	{
		SaveFootPrintReport(reportFile);
	}
}

void MainWindow::on_actionBOM_CSV_triggered()
{
	QFileInfo proj(ui->leProject->text());
	QString bomFile = proj.absoluteFilePath();
	bomFile = bomFile.replace("." + proj.suffix(),"_BOM.csv");

	bomFile = QFileDialog::getSaveFileName(this,
			"Save BOM CSV File",
			bomFile,
			tr("csv Files (*.csv);;All Files (*.*)"));

	if (!bomFile.isEmpty())
	{
		schematic_adder->GenerateBOM(bomFile,ui->leProjectFolder->text());
	}
}

void MainWindow::on_actionClose_triggered()
{
	close();
}

void MainWindow::on_actionAbout_triggered()
{
	auto hpp {helpText.right(helpText.size() - helpText.indexOf("Options:"))};
	QString text {QString("Kicad Helper v%1\nQT v%2\n\n\nCommandline %3\n\nIcons by:\n%4")
		.arg(PROJECT_VER).arg(QT_VERSION_STR)
		.arg(hpp)
		.arg(R"(http://www.famfamfam.com/lab/icons/silk/)")};
		//http://www.famfamfam.com/lab/icons/silk/
	QMessageBox::about( this, "About Kicad Helper", text );
}

void MainWindow::on_actionOpen_Logs_triggered()
{
	QDesktopServices::openUrl(QUrl::fromLocalFile(appdir + "/log/"));
}

void MainWindow::on_pbProjectFolder_clicked()
{
	QDesktopServices::openUrl(QUrl::fromLocalFile(ui->leProjectFolder->text()));
}

void MainWindow::on_pbLibraryFolder_clicked()
{
	QDesktopServices::openUrl(QUrl::fromLocalFile(ui->leLibraryFolder->text()));
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
		LogMessage(QString("Updating Text on %1 ").arg(file), spdlog::level::level_enum::debug, file);
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

void MainWindow::on_pbReloadFPLibraries_clicked()
{
	QDir directory(ui->leProjectFolder->text());
	if (!directory.exists() || ui->leProjectFolder->text().isEmpty())
	{
		LogMessage("Directory Doesn't Exist", spdlog::level::level_enum::warn);
		return;
	}
	ClearFootprintLibrarys();
	footprint_finder->LoadProject(ui->leProjectFolder->text());
}

void MainWindow::on_pbCheckFP_clicked()
{
	ClearFootprintMsgs();
	footprint_finder->CheckSchematics();
}

void MainWindow::on_pbFixFP_clicked()
{
	QDir directory(ui->leLibraryFolder->text());
	if (!directory.exists()|| ui->leLibraryFolder->text().isEmpty())
	{
		LogMessage("Directory Doesn't Exist", spdlog::level::level_enum::warn);
		return;
	}
	footprint_finder->FixFootprints(ui->leLibraryFolder->text());
}

void MainWindow::on_pbReloadSymLibraries_clicked()
{
	QDir directory(ui->leProjectFolder->text());
	if (!directory.exists() || ui->leProjectFolder->text().isEmpty())
	{
		LogMessage("Directory Doesn't Exist", spdlog::level::level_enum::warn);
		return;
	}
	ClearSymbolLibrarys();
	symbol_finder->LoadProject(ui->leProjectFolder->text());
}

void MainWindow::on_pbCheckSym_clicked()
{
	ClearSymbolMsgs();
	symbol_finder->CheckSchematics();
}

void MainWindow::on_pbFixSym_clicked()
{
	QDir directory(ui->leLibraryFolder->text());
	if (!directory.exists()|| ui->leLibraryFolder->text().isEmpty())
	{
		LogMessage("Directory Doesn't Exist", spdlog::level::level_enum::warn);
		return;
	}
	symbol_finder->FixSymbols(ui->leLibraryFolder->text());
}

void MainWindow::on_pbAddFpLibrary_clicked()
{
	QString const footprintLib = QFileDialog::getOpenFileName(this, "Select Kicad Footprint Library Files", ui->leLibraryFolder->text(), tr("Kicad Footprint Librarys (*.kicad_mod *.mod);;All Files (*.*)"));
	if (!footprintLib.isEmpty())
	{
		footprint_finder->ImportLibrary(footprintLib, ui->leLibraryFolder->text() );
	}
}

void MainWindow::on_pbDeleteFpLibrary_clicked()
{
	int row = GetSelectedRow(ui->twProjectFPLibraries);
	if (-1 == row)
	{
		return;
	}
	auto name{ ui->twProjectFPLibraries->item(row, LibraryColumns::Name)->text()};
	footprint_finder->RemoveLibrary(name);
}

void MainWindow::on_pbViewFpLibrary_clicked()
{
	int row = GetSelectedRow(ui->twProjectFPLibraries);
	if (-1 == row)
	{
		return;
	}
	auto list{ footprint_finder->GetFootprints(ui->twProjectFPLibraries->item(row, LibraryColumns::URL)->text(),
		ui->twProjectFPLibraries->item(row, LibraryColumns::Type)->text()) };
	ViewList::Load(list, ui->twProjectFPLibraries->item(row, LibraryColumns::Name)->text());
}

void MainWindow::on_pbViewGlobalFpLibrary_clicked()
{
	int row = GetSelectedRow(ui->twGlobalFPLibraries);
	if (-1 == row)
	{
		return;
	}
	auto list{ footprint_finder->GetFootprints(ui->twGlobalFPLibraries->item(row, LibraryColumns::URL)->text(),
								ui->twGlobalFPLibraries->item(row, LibraryColumns::Type)->text()) };
	ViewList::Load(list, ui->twGlobalFPLibraries->item(row, LibraryColumns::Name)->text());
}

void MainWindow::on_pbOpenGlobalFpLibrary_clicked()
{
	int row = GetSelectedRow(ui->twGlobalFPLibraries);
	if (-1 == row)
	{
		return;
	}
	auto path = QFileInfo(footprint_finder->updatePath(ui->twGlobalFPLibraries->item(row, LibraryColumns::URL)->text())).absoluteDir().absolutePath();
	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::on_pbOpenFpLibrary_clicked()
{
	int row = GetSelectedRow(ui->twProjectFPLibraries);
	if (-1 == row)
	{
		return;
	}
	auto path = QFileInfo(footprint_finder->updatePath(ui->twProjectFPLibraries->item(row, LibraryColumns::URL)->text())).absoluteDir().absolutePath();
	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::on_pbAddSymLibrary_clicked()
{
	QString const symbolLib = QFileDialog::getOpenFileName(this, "Select Kicad Symbol Library Files", ui->leLibraryFolder->text(), tr("Kicad Symbol Librarys (*.kicad_sym *.lib);;All Files (*.*)"));
	if (!symbolLib.isEmpty())
	{
		symbol_finder->ImportLibrary(symbolLib, ui->leLibraryFolder->text());
	}
}

void MainWindow::on_pbDeleteSymLibrary_clicked()
{
	int row = GetSelectedRow(ui->twProjectSymLibraries);
	if (-1 == row)
	{
		return;
	}
	auto name{ ui->twProjectSymLibraries->item(row, LibraryColumns::Name)->text()};
	symbol_finder->RemoveLibrary(name);
}

void MainWindow::on_pbViewSymLibrary_clicked()
{
	int row = GetSelectedRow(ui->twProjectSymLibraries);
	if (-1 == row) 
	{
		return;
	}
	auto list{ symbol_finder->GetSymbols(ui->twProjectSymLibraries->item(row, LibraryColumns::URL)->text(),
								ui->twProjectSymLibraries->item(row, LibraryColumns::Type)->text()) };
	ViewList::Load(list, ui->twProjectSymLibraries->item(row, LibraryColumns::Name)->text());
}

void MainWindow::on_pbOpenSymLibrary_clicked()
{
	int row = GetSelectedRow(ui->twProjectSymLibraries);
	if (-1 == row)
	{
		return;
	}
	auto path = QFileInfo(symbol_finder->updatePath(ui->twProjectSymLibraries->item(row, LibraryColumns::URL)->text())).absoluteDir().absolutePath();
	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::on_pbViewGlobalSymLibrary_clicked()
{
	int row = GetSelectedRow(ui->twGlobalSymLibraries);
	if (-1 == row)
	{
		return;
	}

	auto list{ symbol_finder->GetSymbols(ui->twGlobalSymLibraries->item(row, LibraryColumns::URL)->text(),
									ui->twGlobalSymLibraries->item(row, LibraryColumns::Type)->text()) };
	ViewList::Load(list, ui->twGlobalSymLibraries->item(row, LibraryColumns::Name)->text());
}

void MainWindow::on_pbOpenGlobalSymLibrary_clicked()
{
	int row = GetSelectedRow(ui->twGlobalSymLibraries);
	if (-1 == row)
	{
		return;
	}
	auto path = QFileInfo(symbol_finder->updatePath(ui->twGlobalSymLibraries->item(row, LibraryColumns::URL)->text())).absoluteDir().absolutePath();
	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::on_pbCheck3DModels_clicked() 
{
	ClearThreeDModelMsgs();
	threed_model_finder->CheckPCBs();
}

void MainWindow::on_pbFix3DModels_clicked() 
{
	QDir directory(ui->leLibraryFolder->text());
	if (!directory.exists() || ui->leLibraryFolder->text().isEmpty())
	{
		LogMessage("Directory Doesn't Exist", spdlog::level::level_enum::warn);
		return;
	}
	threed_model_finder->FixThreeDModels(ui->leLibraryFolder->text());
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

void MainWindow::on_pbDeletePN_clicked()
{
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

void MainWindow::on_lwFiles_itemDoubleClicked( QListWidgetItem * item)
{
	if(nullptr == item)
	{
		return;
	}
	QDesktopServices::openUrl(QUrl::fromLocalFile(ui->leProjectFolder->text() + "/" + item->text()));
}

void MainWindow::on_twProjectFPLibraries_cellDoubleClicked(int row, int column)
{
	auto header{ ui->twProjectFPLibraries->horizontalHeaderItem(column)->text() };
	if (column == LibraryColumns::URL)
	{
		auto name{ ui->twProjectFPLibraries->item(row, LibraryColumns::Name)->text() };
		auto value{ ui->twProjectFPLibraries->item(row, LibraryColumns::URL)->text() };

		bool ok;
		QString text = QInputDialog::getText(this, header,
			header, QLineEdit::Normal,
			value, &ok);
		if (ok && !text.isEmpty() && value != text)
		{
			footprint_finder->ChangeLibraryPath(name, text, row);
		}
	}
	else if (column == LibraryColumns::Name)
	{		
		auto value{ ui->twProjectFPLibraries->item(row, LibraryColumns::Name)->text() };

		bool ok;
		QString text = QInputDialog::getText(this, header,
			header, QLineEdit::Normal,
			value, &ok);
		if (ok && !text.isEmpty() && value != text)
		{
			footprint_finder->ChangeLibraryName(value, text, row);
		}
	}
	else if (column == LibraryColumns::Descr)
	{		
		auto name{ ui->twProjectFPLibraries->item(row, LibraryColumns::Name)->text() };
		auto value{ ui->twProjectFPLibraries->item(row, LibraryColumns::Descr)->text() };

		bool ok;
		QString text = QInputDialog::getText(this, header,
			header, QLineEdit::Normal,
			value, &ok);
		if (ok && !text.isEmpty() && value != text)
		{
			footprint_finder->ChangeLibraryDescr(name, text, row);
		}
	}
	else if (column == LibraryColumns::Type)
	{
		auto name{ ui->twProjectSymLibraries->item(row, LibraryColumns::Name)->text() };
		auto value{ ui->twProjectSymLibraries->item(row, LibraryColumns::Type)->text() };

		bool ok;

		//getItem(QWidget * parent, 
		//const QString & title, const QString & label, const QStringList & items, int current = 0, bool editable = true, bool* ok = nullptr, Qt::WindowFlags flags = Qt::WindowFlags(), Qt::InputMethodHints inputMethodHints = Qt::ImhNone)
		QStringList items;
		items << LEGACY_LIB << KICAD_LIB;
		int index = items.indexOf(value);
		QString text = QInputDialog::getItem(this, header,
			header, items,
			index, &ok);
		if (ok && !text.isEmpty() && value != text)
		{
			symbol_finder->ChangeLibraryType(name, text, row);
		}
	}
}

void MainWindow::on_twProjectSymLibraries_cellDoubleClicked(int row, int column)
{
	auto header{ ui->twProjectSymLibraries->horizontalHeaderItem(column)->text() };
	if (column == LibraryColumns::URL) 
	{
		auto name{ ui->twProjectSymLibraries->item(row, LibraryColumns::Name)->text() };
		auto value{ ui->twProjectSymLibraries->item(row, LibraryColumns::URL)->text() };

		bool ok;
		QString text = QInputDialog::getText(this, header,
			header, QLineEdit::Normal,
			value, &ok);
		if (ok && !text.isEmpty() && value != text)
		{
			symbol_finder->ChangeLibraryPath(name, text, row);
		}
	}
	else if (column == LibraryColumns::Name)
	{
		auto value{ ui->twProjectSymLibraries->item(row, LibraryColumns::Name)->text() };
		auto type{ ui->twProjectSymLibraries->item(row, LibraryColumns::Type)->text() };

		bool ok;
		QString text = QInputDialog::getText(this, header,
			header, QLineEdit::Normal,
			value, &ok);
		if (ok && !text.isEmpty() && value != text)
		{
			symbol_finder->ChangeLibraryName(value, text, row);
		}
	}
	else if (column == LibraryColumns::Descr)
	{		
		auto name{ ui->twProjectSymLibraries->item(row, LibraryColumns::Name)->text() };
		auto value{ ui->twProjectSymLibraries->item(row, LibraryColumns::Descr)->text() };

		bool ok;
		QString text = QInputDialog::getText(this, header,
			header, QLineEdit::Normal,
			value, &ok);
		if (ok && !text.isEmpty() && value != text)
		{
			symbol_finder->ChangeLibraryDescr(name, text, row);
		}
	}
	else if (column == LibraryColumns::Type)
	{
		auto name{ ui->twProjectSymLibraries->item(row, LibraryColumns::Name)->text() };
		auto value{ ui->twProjectSymLibraries->item(row, LibraryColumns::Type)->text() };

		bool ok;

		//getItem(QWidget * parent, 
		//const QString & title, const QString & label, const QStringList & items, int current = 0, bool editable = true, bool* ok = nullptr, Qt::WindowFlags flags = Qt::WindowFlags(), Qt::InputMethodHints inputMethodHints = Qt::ImhNone)
		QStringList items;
		items << LEGACY_LIB << KICAD_LIB;
		int index = items.indexOf(value);
		QString text = QInputDialog::getItem(this, header,
			header, items,
			index, &ok);
		if (ok && !text.isEmpty() && value != text)
		{
			symbol_finder->ChangeLibraryType(name, text, row);
		}
	}
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
	settings->setValue("table_index", index);
	settings->sync();
}

void MainWindow::on_lwLogs_itemDoubleClicked(QListWidgetItem * item)
{
	if (item)
	{
		QVariant data = item->data(Qt::UserRole);
		QString filePath = data.toString();
		if(!filePath.isEmpty())
		{
			QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
		}
	}
}

void MainWindow::on_menuRecent_triggered()
{
	auto recentItem = qobject_cast<QAction*>(sender());
	if (recentItem && !recentItem->data().isNull())
	{
		auto const project = qvariant_cast<QString>(recentItem->data());
		SetProject(project);
	}
}

void MainWindow::on_actionClear_triggered()
{
	ui->menuRecent->clear();
	settings->remove("Recent_ProjectsList");
	//settings->setValue("Recent_ProjectsList")

	ui->menuRecent->addSeparator();
	ui->menuRecent->addAction(ui->actionClear);
}

void MainWindow::SetProject(QString const& project)
{
	libraryReport.clear();
	ui->leProject->setText(project);
	settings->setValue("last_project", project);
	settings->sync();

	QFileInfo proj(project);

	ui->leProjectFolder->setText(proj.absoluteDir().absolutePath());

	ui->leOldName->setText(proj.baseName());
	ui->leNewName->setText(proj.baseName());

	QDir directory(proj.absoluteDir().absolutePath());
	auto const& kicadFiles = directory.entryInfoList(QStringList() << "*.kicad_sch" << "*.kicad_pcb" << "*table*", QDir::Files);
	//auto const& kicadFiles = directory.entryInfoList(QStringList() << "*", QDir::Files);
	
	ui->lwFiles->clear();
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

	ClearFootprintLibrarys();
	footprint_finder->LoadProject(proj.absoluteDir().absolutePath());

	ClearSymbolLibrarys();
	symbol_finder->LoadProject(proj.absoluteDir().absolutePath());

	threed_model_finder->LoadProject(proj.absoluteDir().absolutePath());
	AddRecentList(proj.absoluteFilePath());
}

void MainWindow::SetLibrary(QString const& library)
{
	ui->leLibraryFolder->setText(library);
	settings->setValue("last_library", library);
	settings->sync();
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
	ui->twParts->setRowCount(static_cast<int>(schematic_adder->getPartList().size()));
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

	ui->lwWords->setRowCount(static_cast<int>(text_replace->getReplaceList().size()));

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

void MainWindow::AddFootprintLibrary( QString const& level, QString const& name, QString const& type, QString const& descr, QString const& path)
{
	QTableWidget* libraryList{nullptr};
	if(PROJECT_LIB == level)
	{
		libraryList = ui->twProjectFPLibraries;
	}
	else if(GLOBAL_LIB == level)
	{
		libraryList = ui->twGlobalFPLibraries;
	}
	else
	{
		return;
	}
	AddLibraryItem(libraryList, name, type, descr, path);
}

void MainWindow::UpdateFootprintLibraryRow(QString const& level, QString const& name, QString const& type, QString const& descr, QString const& path, int row)
{
	auto SetItem = [&](int row, int col, QString const& text)
	{
		ui->twProjectFPLibraries->item(row, col)->setText(text);
	};

	SetItem(row, LibraryColumns::Name, name);
	SetItem(row, LibraryColumns::Type, type);
	SetItem(row, LibraryColumns::Descr, descr);
	SetItem(row, LibraryColumns::URL, path);

	ui->twProjectFPLibraries->resizeColumnsToContents();
}

void MainWindow::ClearFootprintLibrary(QString const& level)
{
	if(PROJECT_LIB == level)
	{
		ClearTableWidget(ui->twProjectFPLibraries);
	}
	if(GLOBAL_LIB == level)
	{
		ClearTableWidget(ui->twGlobalFPLibraries);
	}
}

void MainWindow::SetFootprintLibraryError(QString const& level, QString const& name) 
{
	if (PROJECT_LIB == level)
	{
		SetTableWidgetError(ui->twProjectFPLibraries, name);
	}
	if (GLOBAL_LIB == level)
	{
		SetTableWidgetError(ui->twGlobalFPLibraries, name);
	}
}

void MainWindow::ClearFootprintLibrarys()
{
	ClearTableWidget(ui->twGlobalFPLibraries);
	ClearTableWidget(ui->twProjectFPLibraries);
}

void MainWindow::AddFootprintMsg( QString const& message, bool error)
{
	AddResultMsg(ui->lwFPResults, message ,error);
}

void MainWindow::AddResultMsg( QListWidget * listw, QString const& message, bool error)
{
	libraryReport.append(message);
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
	listw->addItem(it);
	listw->scrollToBottom();
}

void MainWindow::AddSymbolLibrary(QString const& level, QString const& name, QString const& type, QString const& descr, QString const& path)
{
	QTableWidget* libraryList{nullptr};
	if(PROJECT_LIB == level)
	{
		libraryList = ui->twProjectSymLibraries;
	}
	else if(GLOBAL_LIB == level)
	{
		libraryList = ui->twGlobalSymLibraries;
	}
	else
	{
		return;
	}
	AddLibraryItem(libraryList, name, type, descr, path);
}

void MainWindow::UpdateSymbolLibraryRow(QString const& level, QString const& name, QString const& type, QString const& descr, QString const& path, int row)
{
	auto SetItem = [&](int row, int col, QString const& text)
	{
		ui->twProjectSymLibraries->item(row, col)->setText(text);
	};

	SetItem(row, LibraryColumns::Name, name);
	SetItem(row, LibraryColumns::Type, type);
	SetItem(row, LibraryColumns::Descr, descr);
	SetItem(row, LibraryColumns::URL, path);

	ui->twProjectSymLibraries->resizeColumnsToContents();
}

void MainWindow::ClearSymbolLibrary(QString const& level)
{
	if(PROJECT_LIB == level)
	{
		ClearTableWidget(ui->twProjectSymLibraries);
	}
	if(GLOBAL_LIB == level)
	{
		ClearTableWidget(ui->twGlobalSymLibraries);
	}
}
void MainWindow::SetSymbolLibraryError(QString const& level, QString const& name) 
{
	if (PROJECT_LIB == level)
	{
		SetTableWidgetError(ui->twProjectSymLibraries, name);
	}
	if (GLOBAL_LIB == level)
	{
		SetTableWidgetError(ui->twGlobalSymLibraries, name);
	}
}

void MainWindow::ClearSymbolLibrarys()
{
	ClearTableWidget(ui->twGlobalSymLibraries);
	ClearTableWidget(ui->twProjectSymLibraries);
}

void MainWindow::ClearTableWidget(QTableWidget* table)
{
	table->clearContents();
	while( table->rowCount() != 0 )
	{
		table->removeRow(0);
	}
}

void MainWindow::SetTableWidgetError(QTableWidget* table , QString const& name)
{
	for (int i =0; i < table->rowCount(); ++i)
	{
		if (table->item(i, LibraryColumns::Name)->text() == name)
		{
			for (int j = 0; j < table->columnCount(); ++j)
			{
				table->item(i, j)->setBackground(Qt::yellow);
			}
			return;
		}
	}
}

void MainWindow::AddSymbolMsg( QString const& message, bool error)
{
	AddResultMsg(ui->lwSymResults, message ,error);
}

void MainWindow::ClearSymbolMsgs()
{
	ui->lwSymResults->clear();
	for (int i = 0; i < ui->twProjectSymLibraries->rowCount(); ++i)
	{		
		for (int j = 0; j < ui->twProjectSymLibraries->columnCount(); ++j)
		{
			ui->twProjectSymLibraries->item(i, j)->setBackground(ui->twProjectSymLibraries->palette().base().color());
		}
	}
	for (int i = 0; i < ui->twGlobalSymLibraries->rowCount(); ++i)
	{
		for (int j = 0; j < ui->twGlobalSymLibraries->columnCount(); ++j)
		{
			ui->twGlobalSymLibraries->item(i, j)->setBackground(ui->twGlobalSymLibraries->palette().base().color());
		}
	}
}

void MainWindow::ClearFootprintMsgs()
{
	ui->lwFPResults->clear();
	for (int i = 0; i < ui->twProjectFPLibraries->rowCount(); ++i)
	{
		for (int j = 0; j < ui->twProjectFPLibraries->columnCount(); ++j)
		{
			ui->twProjectFPLibraries->item(i, j)->setBackground(ui->twProjectFPLibraries->palette().base().color());
		}
	}
	for (int i = 0; i < ui->twGlobalFPLibraries->rowCount(); ++i)
	{
		for (int j = 0; j < ui->twGlobalFPLibraries->columnCount(); ++j)
		{
			ui->twGlobalFPLibraries->item(i, j)->setBackground(ui->twGlobalFPLibraries->palette().base().color());
		}
	}
}

void MainWindow::AddThreeDModelMsg(QString const& message, bool error)
{
	AddResultMsg(ui->lw3DModelsResults, message, error);
}
void MainWindow::ClearThreeDModelMsgs() 
{
	ui->lw3DModelsResults->clear();
}

void MainWindow::AddLibraryItem(QTableWidget* libraryList, QString const& name, QString const& type, QString const& descr, QString const& path)
{
	auto SetItem = [&](int row, int col, QString const& text)
	{
		libraryList->setItem(row, col, new QTableWidgetItem());
		libraryList->item(row, col)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		libraryList->item(row, col)->setText(text);
	};

	int row = libraryList->rowCount();
	libraryList->insertRow(row);

	SetItem(row, LibraryColumns::Name, name);
	SetItem(row, LibraryColumns::Type, type);
	SetItem(row, LibraryColumns::Descr, descr);
	SetItem(row, LibraryColumns::URL, path);

	libraryList->resizeColumnsToContents();
}

int MainWindow::GetSelectedRow(QTableWidget* table) const
{
	return table->selectionModel()->currentIndex().row();
}

void MainWindow::AddRecentList(QString const& file)
{
	auto recentProjectList = settings->value("Recent_ProjectsList").toStringList();

	recentProjectList.push_front(file);
	recentProjectList.removeDuplicates();
	if (recentProjectList.size() > 10)
	{
		recentProjectList.pop_back();
	}
	settings->setValue("Recent_ProjectsList", recentProjectList);
	settings->sync();
	RedrawRecentList();
}

void MainWindow::RedrawRecentList()
{
	ui->menuRecent->clear();
	auto recentProjectList = settings->value("Recent_ProjectsList").toStringList();
	for (auto const& file : recentProjectList)
	{
		QFileInfo fileInfo(file);
		auto* recentpn = new QAction(this);
		recentpn->setText(fileInfo.dir().dirName() + "/" + fileInfo.fileName());
		recentpn->setData(fileInfo.absoluteFilePath());
		ui->menuRecent->addAction(recentpn);
		connect(recentpn, &QAction::triggered, this, &MainWindow::on_menuRecent_triggered);
	}

	ui->menuRecent->addSeparator();
	ui->menuRecent->addAction(ui->actionClear);
}

void MainWindow::SaveFootPrintReport(QString const& fileName)
{
	QFile outFile(fileName);
	if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		LogMessage(QString("Could not Open '%1'").arg(fileName), spdlog::level::level_enum::warn, fileName);
		return;
	}
	QTextStream out(&outFile);
	for (auto const& line : libraryReport)
	{
		out << line << "\n";
	}	
	outFile.close();
	LogMessage(QString("Saved Report to '%1'").arg(outFile.fileName()), spdlog::level::level_enum::debug, fileName);
}

void MainWindow::LogMessage(QString const& message, spdlog::level::level_enum llvl, QString const& file)
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
	msgColors.append("#DC582A");
	msgColors.append(Qt::darkRed);
	msgColors.append(Qt::red);

	QListWidgetItem* it = new QListWidgetItem(message);
	it->setForeground(QBrush(QColor(msgColors.at(llvl))));
	it->setData(Qt::UserRole, file);
	ui->lwLogs->addItem(it);
	ui->lwLogs->scrollToBottom();

	qApp->processEvents();
}

void MainWindow::ProcessCommandLine()
{
	QCommandLineParser parser;
    parser.setApplicationDescription("Kicad Helper");

    QCommandLineOption projectOption(QStringList() << "p" << "project",
             "Project File to Load.",
            "project");
    parser.addOption(projectOption);

    QCommandLineOption mappingOption(QStringList() << "m" << "mapping",
             "Text Replace Mapping JSON to Load.",
            "mapping");
    parser.addOption(mappingOption);

    QCommandLineOption partlistOption(QStringList() << "t" << "partlist",
             "Part Number List JSON to Load.",
            "partlist");
    parser.addOption(partlistOption);

	QCommandLineOption libraryOption(QStringList() << "l" << "library",
		"Set Library Folder.",
		"library");
	parser.addOption(libraryOption);

	QCommandLineOption bomOption(QStringList() << "b" << "bom",
		"Export to CSV BOM.",
		"bom");
	parser.addOption(bomOption);

	QCommandLineOption reportOption(QStringList() << "o" << "report",
             "Save Check Schematic FootPrints Report.",
            "report");
    parser.addOption(reportOption);

	QCommandLineOption addOption(QStringList() << "a" << "add",
            "Add Parts Numbers to Schematic.");
	parser.addOption(addOption);

	QCommandLineOption checkOption(QStringList() << "c" << "check",
            "Check Schematic Symbols and Footprints.");
    parser.addOption(checkOption);

	QCommandLineOption fixOption(QStringList() << "f" << "find",
		"Attempt to Find Symbols and Footprints.");
	parser.addOption(fixOption);

	QCommandLineOption checkSymOption(QStringList() << "s" << "checksym",
            "Check Schematic Symbols.");
    parser.addOption(checkSymOption);

	QCommandLineOption checkFpOption(QStringList() << "n" << "checkfp",
            "Check Schematic Footprints.");
    parser.addOption(checkFpOption);

	QCommandLineOption check3dOption(QStringList() << "3" << "check3d",
            "Check 3D Model Paths.");
    parser.addOption(check3dOption);

	QCommandLineOption fixSymOption(QStringList() << "y" << "findsym",
		"Attempt to Find Schematic Symbols.");
	parser.addOption(fixSymOption);

	QCommandLineOption fixFpOption(QStringList() << "i" << "findfp",
		"Attempt to Find Schematic Footprints.");
	parser.addOption(fixFpOption);

	QCommandLineOption fix3dOption(QStringList() << "d" << "find3d",
            "Fix 3D Model Paths.");
    parser.addOption(fix3dOption);

	QCommandLineOption replaceOption(QStringList() << "r" << "replace",
            "File to do text Repace in.",
			"replace");

    parser.addOption(replaceOption);

	QCommandLineOption exitOption(QStringList() << "x" << "exit",
            "Exit Software when done.");
    parser.addOption(exitOption);

	helpText = parser.helpText();

    parser.parse(QCoreApplication::arguments());

	auto lastProject{ settings->value("last_project").toString() };

	if(!parser.value(partlistOption).isEmpty() && QFile::exists(parser.value(partlistOption)))
	{
		schematic_adder->LoadJsonFile(parser.value(partlistOption));
	}
	else if (QFile::exists(appdir + "/part_numbers.json"))
	{
		schematic_adder->LoadJsonFile(appdir + "/part_numbers.json");
	}

	if(!parser.value(mappingOption).isEmpty() && QFile::exists(parser.value(mappingOption)))
	{
		text_replace->LoadJsonFile(parser.value(mappingOption));
	}
	else if (QFile::exists(appdir + "/mapping.json"))
	{
		text_replace->LoadJsonFile(appdir + "/mapping.json");
	}

	if (!parser.value(libraryOption).isEmpty() && QDir(parser.value(libraryOption)).exists())
	{
		SetLibrary(parser.value(libraryOption));
	}

	else if (QDir(settings->value("last_library").toString()).exists())
	{
		SetLibrary(settings->value("last_library").toString());
	}

	if(!parser.value(projectOption).isEmpty() && QFile::exists(parser.value(projectOption)))
	{
		SetProject(parser.value(projectOption));
	}
	else if (QFile::exists(lastProject))
	{
		SetProject(lastProject);
	}

	if(!parser.value(replaceOption).isEmpty() && QFile::exists(parser.value(replaceOption)))
	{
		ReplaceInFile(parser.value(replaceOption), text_replace->getReplaceList());
	}

	if(parser.isSet(addOption))
	{
		on_pbAddPN_clicked();
	}

	if(parser.isSet(checkOption) || parser.isSet(checkFpOption))
	{
		on_pbCheckFP_clicked();
	}

	if (parser.isSet(fixOption) || parser.isSet(fixFpOption))
	{
		on_pbFixFP_clicked();
	}

	if(parser.isSet(checkOption) || parser.isSet(checkSymOption))
	{
		on_pbCheckSym_clicked();
	}

	if (parser.isSet(fixOption) || parser.isSet(fixSymOption))
	{
		on_pbFixSym_clicked();
	}

	if(parser.isSet(checkOption) || parser.isSet(check3dOption))
	{
		on_pbCheck3DModels_clicked();
	}

	if (parser.isSet(fixOption) || parser.isSet(fix3dOption))
	{
		on_pbFix3DModels_clicked();
	}

	if (!parser.value(bomOption).isEmpty())
	{
		schematic_adder->GenerateBOM(parser.value(bomOption),ui->leProjectFolder->text());
	}

	if(!parser.value(reportOption).isEmpty())
	{
		SaveFootPrintReport(parser.value(reportOption));
	}

	if(parser.isSet(exitOption))
	{
		close();
	}
}
