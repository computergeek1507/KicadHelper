#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "spdlog/spdlog.h"

#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }

class QSettings;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public Q_SLOTS:

    void on_actionOpen_Project_triggered();
    void on_actionOpen_Logs_triggered();
    void on_actionClose_triggered();

    //1st tab
    void on_pbProject_clicked();
    void on_pbRename_clicked();
    //2nd tab
    void on_pbFindSchematic_clicked();
    void on_pbImportCSV_clicked();
    void on_pbTextReplace_clicked();
    //3st tab
    void on_pbFindPCB_clicked();
    void on_pbCheckFP_clicked();
    //4nd tab
    void on_pbFindProjet_clicked();
    void on_pbImportParts_clicked();
    void on_pbSetPartsInSch_clicked();

private:
    Ui::MainWindow *ui;

    std::shared_ptr<spdlog::logger> logger{ nullptr };
    std::unique_ptr<QSettings> settings{ nullptr };

    void SetProject(QString const& project);
};
#endif // MAINWINDOW_H
