#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

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

private:
    Ui::MainWindow* ui;
    QMap<QDockWidget*, QString> m_dockNames;

    void loadStyleSheet();
    void setupDockWidgets();
    void setupMenuBar();
    void setupStatusBar();
    void createDockWidget(const QString& title,
        QWidget* widget,
        Qt::DockWidgetArea area);

    LispEditor* createLispEditor();
    QTreeWidget* createProjectTree();
    Console* createREPLConsole();
    QListWidget* createSymbolTable();
};

#endif