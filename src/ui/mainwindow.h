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

    void openProject();
    void createProject();


private:
    Ui::MainWindow* ui;
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

    // Меню для тем приложения
    QAction* m_lightStyleAction = nullptr;
    QAction* m_darkStyleAction = nullptr;

    // Меню правка
    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_cutAction;
    QAction* m_copyAction;
    QAction* m_pasteAction;

    void loadTheme(QString stylePath);

    void setupDockWidgets();
    void setupMenuBar();
    void setupStatusBar();
    void setupConnections();

    ProjectTree* createProjectTree();
    Console* createConsoleLisp();
};

#endif