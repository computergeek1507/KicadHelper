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

class LibraryFinder;
class SchematicAdder;
class TextReplace;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public Q_SLOTS:

    void on_actionOpen_Project_triggered();
    void on_actionReload_Project_triggered();
    void on_actionImport_PartList_triggered();
    void on_actionImport_Rename_Map_triggered();
    void on_actionOpen_Logs_triggered();
    void on_actionClose_triggered();

    void on_pbProjectFolder_clicked();

    //1st tab
    void on_pbTextReplace_clicked();
    void on_pbAddMap_clicked();
    void on_pbRemoveMap_clicked();
    //2nd tab
    void on_pbReloadLibraries_clicked();
    void on_pbCheckFP_clicked();
    //3rd tab
    void on_pbSetPartsInSch_clicked();
    void on_pbAddPN_clicked();
    void on_pbRemovePN_clicked();
    //4nd tab
    void on_pbRename_clicked();

    void on_lwWords_cellDoubleClicked(int row, int column);
    void on_twParts_cellDoubleClicked(int row, int column);

    void LogMessage(QString const& message , spdlog::level::level_enum llvl = spdlog::level::level_enum::debug);

    void AddLibrary(QString const& level, QString const& name, QString const& type, QString const& path);

    void ClearLibrarys();

    void AddFootPrintMsg( QString const& message, bool error);

    void RedrawPartList(bool save);
    void RedrawMappingList(bool save);

private:
    Ui::MainWindow *ui;

    std::shared_ptr<spdlog::logger> logger{ nullptr };
    std::unique_ptr<QSettings> settings{ nullptr };
    std::unique_ptr<LibraryFinder> library_finder{ nullptr };
    std::unique_ptr<SchematicAdder> schematic_adder{ nullptr };
    std::unique_ptr<TextReplace> text_replace{ nullptr };


    QString appdir;

    void SetProject(QString const& project);

    void ReplaceInFile(QString const& filePath, std::vector<std::pair<QString, QString>> const& replaceList);
    void CopyRecursive(const std::filesystem::path& src, const std::filesystem::path& target) ;
    void MoveRecursive(const std::filesystem::path& src, const std::filesystem::path& target, bool createRoot = true);
};
#endif // MAINWINDOW_H
