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
#include <QCommandLineParser>
#include <QTimer>

#include "spdlog/spdlog.h"

#include "spdlog/sinks/qt_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"

#include <filesystem>

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
		//rotating->set_level(spdlog::level::debug);
		//rotating->flush

		logger = std::make_shared<spdlog::logger>("kicadhelper", rotating);
		logger->flush_on(spdlog::level::debug);
		logger->set_level(spdlog::level::debug);
		logger->set_pattern("[%D %H:%M:%S] [%L] %v");

		//spdlog::logger logger("multi_sink", { console_sink, file_sink });
	}
	catch (std::exception& /*ex*/)
	{
		QMessageBox::warning(this, "Logger Failed", "Logger Failed To Start.");
	}

	//logger->info("Program started v" + std::string( PROJECT_VER));
	//logger->info(std::string("Built with Qt ") + std::string(QT_VERSION_STR));

	setWindowTitle(windowTitle() + " v" + PROJECT_VER);

	settings = std::make_unique< QSettings>(appdir + "/settings.ini", QSettings::IniFormat);

	library_finder = std::make_unique<LibraryFinder>();
	connect( library_finder.get(), &LibraryFinder::SendMessage, this, &MainWindow::LogMessage );
	connect( library_finder.get(), &LibraryFinder::AddLibrary, this, &MainWindow::AddLibrary );
	connect( library_finder.get(), &LibraryFinder::ClearLibrary, this, &MainWindow::ClearLibrary );
	connect( library_finder.get(), &LibraryFinder::SendResult, this, &MainWindow::AddFootPrintMsg );
	connect( library_finder.get(), &LibraryFinder::ClearResults, this, &MainWindow::ClearFootPrintMsgs );

	schematic_adder = std::make_unique<SchematicAdder>();
	connect( schematic_adder.get(), &SchematicAdder::SendMessage, this, &MainWindow::LogMessage );
	connect(schematic_adder.get(), &SchematicAdder::RedrawPartList, this, &MainWindow::RedrawPartList);
	connect(schematic_adder.get(), &SchematicAdder::UpdatePartRow, this, &MainWindow::UpdatePartRow);

	text_replace = std::make_unique<TextReplace>();
	connect(text_replace.get(), &TextReplace::SendMessage, this, &MainWindow::LogMessage);
	connect(text_replace.get(), &TextReplace::RedrawTextReplace, this, &MainWindow::RedrawMappingList);
	connect(text_replace.get(), &TextReplace::UpdateTextRow, this, &MainWindow::UpdateMappingRow);

	bool overrideImport = settings->value("override", false).toBool();

	ui->actionOverride->setChecked(overrideImport);

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
	QStringList const schList = QFileDialog::getOpenFileNames(this, "Select Kicad Schematic Files",settings->value("last_project").toString(), tr("Kicad Schematic Files (*.kicad_sch);;All Files (*.*)"));
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

void MainWindow::on_actionClose_triggered()
{
	close();
}

void MainWindow::on_actionAbout_triggered()
{
	auto hpp {helpText.right(helpText.size() - helpText.indexOf("Options:"))};
	QString text {QString("Kicad Helper v%1\nQT v%2\n\n\n%3").arg(PROJECT_VER).arg(QT_VERSION_STR).arg(hpp)};

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
	if (!directory.exists() || ui->leProjectFolder->text().isEmpty())
	{
		LogMessage("Directory Doesn't Exist", spdlog::level::level_enum::warn);
		return;
	}
	ClearLibrarys();
	library_finder->LoadProject(ui->leProjectFolder->text());
}

void MainWindow::on_pbCheckFP_clicked()
{
	ClearFootPrintMsgs();
	library_finder->CheckSchematics();
}

void MainWindow::on_pbFixFP_clicked()
{
	QDir directory(ui->leLibraryFolder->text());
	if (!directory.exists()|| ui->leLibraryFolder->text().isEmpty())
	{
		LogMessage("Directory Doesn't Exist", spdlog::level::level_enum::warn);
		return;
	}
	library_finder->FixFootPrints(ui->leLibraryFolder->text());
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

	ClearLibrarys();
	library_finder->LoadProject(proj.absoluteDir().absolutePath());
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
	QTableWidget* libraryList{nullptr};
	if(PROJECT_LIB==level)
	{
		libraryList = ui->twProjectLibraries;
		
		
	}else if(SYSTEM_LIB==level)
	{
		libraryList = ui->twGlobalLibraries;
	}
	else
	{
		return;
	}
	auto SetItem = [&](int row, int col, QString const& text)
	{
		libraryList->setItem(row, col, new QTableWidgetItem());
		libraryList->item(row, col)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		libraryList->item(row, col)->setText(text);
	};

	int row = libraryList->rowCount();
	libraryList->insertRow(row);

	SetItem(row, 0, name);
	SetItem(row, 1, type);
	SetItem(row, 2, path);

	libraryList->resizeColumnsToContents();
}

void MainWindow::ClearLibrary(QString const& level)
{
	if(PROJECT_LIB==level)
	{
		ui->twProjectLibraries->clearContents();
		while( ui->twProjectLibraries->rowCount() != 0 )
		{
			ui->twProjectLibraries->removeRow(0);
		}
		
	}
	if(SYSTEM_LIB==level)
	{
		ui->twGlobalLibraries->clearContents();
		while( ui->twGlobalLibraries->rowCount() != 0 )
		{
			ui->twGlobalLibraries->removeRow(0);
		}
	}
}

void MainWindow::ClearLibrarys()
{
	ui->twGlobalLibraries->clearContents();
	while( ui->twGlobalLibraries->rowCount() != 0 )
	{
		ui->twGlobalLibraries->removeRow(0);
	}

	ui->twProjectLibraries->clearContents();
	while( ui->twProjectLibraries->rowCount() != 0 )
	{
		ui->twProjectLibraries->removeRow(0);
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

void MainWindow::ClearFootPrintMsgs()
{
	ui->lwResults->clear();
}

void MainWindow::SaveFootPrintReport(QString const& fileName)
{
	QFile outFile(fileName);
	if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		LogMessage(QString("Could not Open '%1'").arg(fileName), spdlog::level::level_enum::warn);
		return;
	}
	QTextStream out(&outFile);
	for (int i = 0; i< ui->lwResults->count(); ++i)
	{
		out << ui->lwResults->item(i)->text() << "\n";
	}	
	outFile.close();
	LogMessage(QString("Saved Report to '%1'").arg(outFile.fileName()));
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

    QCommandLineOption partlistOption(QStringList() << "l" << "partlist",
             "Part Number List JSON to Load.",
            "partlist");
    parser.addOption(partlistOption);

	QCommandLineOption libraryOption(QStringList() << "b" << "library",
		"Set Library Folder.",
		"library");

	parser.addOption(libraryOption);

	QCommandLineOption reportOption(QStringList() << "o" << "report",
             "Save Check Schematic FootPrints Report.",
            "report");
    parser.addOption(reportOption);

	QCommandLineOption addOption(QStringList() << "a" << "add",
            "Add Parts Numbers to Schematic.");
	parser.addOption(addOption);

	QCommandLineOption checkOption(QStringList() << "c" << "check",
            "Check Schematic FootPrints.");
    parser.addOption(checkOption);

	QCommandLineOption fixOption(QStringList() << "f" << "find",
		"Attempt to Find Schematic FootPrints.");
	parser.addOption(fixOption);

	QCommandLineOption replaceOption(QStringList() << "r" << "replace",
            "File to Repace Values in.",
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

	if(parser.isSet(checkOption))
	{
		on_pbCheckFP_clicked();
	}

	if (parser.isSet(fixOption))
	{
		on_pbFixFP_clicked();
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
