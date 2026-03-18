#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

#include "ui/widgets/editor_tab_widget.h"
#include "ui/widgets/project_tree.h"
#include "ui/widgets/console.h"
#include "ui/about_dialog.h"
#include "settings.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QTextEdit;
class QTreeWidget;
class QListWidget;
class QDockWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void openFiles(QStringList files);

signals:
    void themeChanged(QString& themeName);

private slots:
    void saveAppState();
    void loadAppState();

private:
    Ui::MainWindow* ui;
    QToolBar* m_mainToolBar;

    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupDockWidgets();
    void loadTheme(QString stylePath);

    ProjectTree* createProjectTree();
    Console* createConsoleLisp();

    void openProject();
    void createProject();

    QMap<QDockWidget*, QString> m_dockNames;

    QDockWidget* m_projectDock = nullptr;
    QDockWidget* m_ConsoleDock = nullptr;

    // Основные виджеты
    EditorTabWidget* m_tabWidget;
    ProjectTree* m_projectTree;
    Console* m_console;

    // Меню работы с файлами
    QAction* m_createFileAction = nullptr;
    QAction* m_openFileAction = nullptr;
    QAction* m_openProjectAction = nullptr;
    QAction* m_newProjectAction = nullptr;
    QAction* m_saveFileAction = nullptr;
    QAction* m_saveFileAsAction = nullptr;
    QAction* m_saveFileAllAction = nullptr;

    // Меню для тем приложения
    QAction* m_lightStyleAction = nullptr;
    QAction* m_darkStyleAction = nullptr;

    // Меню правка
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_cutAction = nullptr;
    QAction* m_copyAction = nullptr;
    QAction* m_pasteAction = nullptr;

    // Запуск
    QAction* m_startReplAction = nullptr;

    
};

#endif