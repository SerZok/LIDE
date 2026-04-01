#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

#include "ui/widgets/editor_tab_widget.h"
#include "ui/widgets/project_tree.h"
#include "ui/widgets/repl_widget.h"
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
    void updateStatusBarPosition(int line, int col);

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

    LispEditor* findEditorByFile(const QString& filePath);
    void onLispError(const QString& file, const QString& message, int line, int column);

    ProjectTree* createProjectTree();
    ReplWidget* createConsoleLisp();

    void openProject();
    void createProject();

    QMap<QDockWidget*, QString> m_dockNames;

    QDockWidget* m_projectDock = nullptr;
    QDockWidget* m_ConsoleDock = nullptr;

    // Основные виджеты
    EditorTabWidget* m_tabWidget;
    ProjectTree* m_projectTree;
    ReplWidget* m_console;

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
    QAction* m_runAction = nullptr;
    QAction* m_restartAction = nullptr;
    QAction* m_cleanRunAction = nullptr;

    // StatusBar
    QLabel* m_positionLabel;
    
};

#endif