#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "partinfo.h"

#include "spdlog/spdlog.h"
#include "spdlog/common.h"

#include <memory>
#include <filesystem>

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
    std::vector<std::pair<QString, QString>> replaceWordsList;

    std::vector<PartInfo> partList;

    void SetProject(QString const& project);
    void ImportRenameCSV(QString const& csvFile);
    void ImportPartNumerCSV(QString const& csvFile);

    void ReplaceInFile(QString const& filePath, std::vector<std::pair<QString, QString>> const& replaceList);
    void CopyRecursive(const std::filesystem::path& src, const std::filesystem::path& target) ;
    void MoveRecursive(const std::filesystem::path& src, const std::filesystem::path& target, bool createRoot = true);
    void UpdateSchematic(QString const& schPath);
    //void LogMessage(std::string const& message, spdlog::level::level_enum llvl = spdlog::level::level_enum::debug)
    //{
    //    LogMessage(QString::fromStdString(message), llvl);
    //}
    void LogMessage(QString const& message , spdlog::level::level_enum llvl = spdlog::level::level_enum::debug);
};
#endif // MAINWINDOW_H
