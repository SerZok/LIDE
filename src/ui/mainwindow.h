#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

#include "ui/widgets/project_tree.h"
#include "ui/widgets/lisp_editor.h"
#include "ui/widgets/console.h"

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

private slots:
    void saveLayout();
    void loadLayout();
    void toggleDockWidget(QDockWidget* dock);
    void onDockLocationChanged(Qt::DockWidgetArea area);

    void openProject();
    void createProject();

private:
    Ui::MainWindow* ui;
    QMap<QDockWidget*, QString> m_dockNames;

    // Основные виджеты
    LispEditor* m_lispEditor;
    ProjectTree* m_projectTree;
    Console* m_console;

    // Меню работы с файлами
    QAction* m_openFileAction = nullptr;
    QAction* m_openProjectAction = nullptr;
    QAction* m_newProjectAction = nullptr;
    QAction* m_saveFileAction = nullptr;
    QAction* m_saveFileAsAction = nullptr;

    void loadStyleSheet();
    void setupDockWidgets();
    void setupMenuBar();
    void setupStatusBar();
    void setupConnections();

    void createDockWidget(const QString& title,
        QWidget* widget,
        Qt::DockWidgetArea area);

    ProjectTree* createProjectTree();
    Console* createREPLConsole();
    QListWidget* createSymbolTable();
};

#endif