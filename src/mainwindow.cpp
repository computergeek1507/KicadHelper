#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QMessageBox>
#include <QDesktopServices>
#include <QSettings>
#include <QFileDialog>

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
		//rotating->set_level(spdlog::level::trace);
		//rotating->flush
		
		logger = std::make_shared<spdlog::logger>("kicadhelper", rotating);
		logger->flush_on(spdlog::level::debug);

		//spdlog::logger logger("multi_sink", { console_sink, file_sink });
	}
	catch (std::exception& /*ex*/)
	{
		QMessageBox::warning(this, "Logger Failed", "Logger Failed To Start.");
	}

	logger->debug("Program started");
	logger->debug(std::string("Built with Qt ") + std::string(QT_VERSION_STR));

	settings = std::make_unique< QSettings>(dir + "settings.ini", QSettings::IniFormat);

	auto lastProject{ settings->value("last_project").toString() };

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

}

void MainWindow::on_pbFindSchematic_clicked()
{

}

void MainWindow::on_pbImportCSV_clicked()
{

}

void MainWindow::on_pbTextReplace_clicked()
{

}

void MainWindow::on_pbFindPCB_clicked()
{

}

void MainWindow::on_pbCheckFP_clicked()
{

}

void MainWindow::on_pbFindProjet_clicked()
{

}

void MainWindow::on_pbImportParts_clicked()
{

}

void MainWindow::on_pbSetPartsInSch_clicked() 
{

}

void MainWindow::SetProject(QString const& project)
{
	ui->leProject->setText(project);
	settings->setValue("last_project", project);

	QFileInfo proj(project);

	ui->leProjectFolder->setText(proj.absoluteDir().absolutePath());

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

	QStringList const& csvFiles = directory.entryList(QStringList() << "*.csv", QDir::Files);
	for (auto const& csv : csvFiles) {
		if (csv.contains("partnumbers") || csv.contains("part_numbers"))
		{
			ui->lePartsCSV->setText(csv);
		}
		if (csv.contains("map") || csv.contains("rename"))
		{
			ui->leMapCSV->setText(csv);
		}
	}
}