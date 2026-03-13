#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

#include "ui/widgets/editor_tab_widget.h"
#include "ui/widgets/project_tree.h"
#include "ui/widgets/console.h"
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

    // Основные виджеты
    EditorTabWidget* m_tabWidget;
    ProjectTree* m_projectTree;
    Console* m_console;

    // Меню работы с файлами
    QAction* m_openFileAction = nullptr;
    QAction* m_openProjectAction = nullptr;
    QAction* m_newProjectAction = nullptr;
    QAction* m_saveFileAction = nullptr;
    QAction* m_saveFileAsAction = nullptr;

    // Меню для тем приложения
    QAction* m_lightStyleAction = nullptr;
    QAction* m_darkStyleAction = nullptr;

    void loadTheme(QString stylePath);
    void openFiles(QStringList files);

    void setupDockWidgets();
    void setupMenuBar();
    void setupStatusBar();
    void setupConnections();

    void createDockWidget(const QString& title,
        QWidget* widget,
        Qt::DockWidgetArea area);

    ProjectTree* createProjectTree();
    Console* createConsoleLisp();
};

#endif