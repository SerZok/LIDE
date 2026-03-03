#include <QTextEdit>
#include <QTreeWidget>
#include <QListWidget>
#include <QDockWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QSplitter>
#include <QLabel>
#include <QFontDatabase>
#include <QMessageBox>
#include <QFile>

#include "mainwindow.h"
#include "ui/ui_mainwindow.h"

MainWindow::MainWindow(QWidget* parent): QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("LIDE - Lisp IDE");
    resize(1366, 1080);

    loadStyleSheet();
    setupDockWidgets();
    setupMenuBar();
    setupStatusBar();

    loadLayout();
}

MainWindow::~MainWindow()
{
    saveLayout();
    delete ui;
}

void MainWindow::loadStyleSheet() {
    QFile file(":/styles/style.css");
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(file.readAll());
        qApp->setStyleSheet(styleSheet);
        file.close();
    }
    else {
        qDebug() << "Ошибка загрузки стилей!";
    }
}

void MainWindow::setupDockWidgets()
{
    // 1. Центральный редактор (не док, а центральный виджет)
    auto* centralEditor = createLispEditor();
    setCentralWidget(centralEditor);

    // 2. Дерево проекта (слева)
    createDockWidget(tr("Дерево проекта"),
        createProjectTree(),
        Qt::LeftDockWidgetArea);

    // 3. REPL консоль (снизу)
    createDockWidget(tr("Консоль REPL"),
        createREPLConsole(),
        Qt::BottomDockWidgetArea);

    // 4. Список символов/переменных (справа)
    createDockWidget(tr("Список переменных"),
        createSymbolTable(),
        Qt::RightDockWidgetArea);
}

void MainWindow::createDockWidget(const QString& title, QWidget* widget, Qt::DockWidgetArea area)
{
    auto* dock = new QDockWidget(title, this);
    dock->setWidget(widget);
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(area, dock);

    m_dockNames[dock] = title;
    connect(dock, &QDockWidget::dockLocationChanged, this, &MainWindow::onDockLocationChanged);
}

LispEditor* MainWindow::createLispEditor()
{
    auto* editor = new LispEditor();
    return editor;
}

QTreeWidget* MainWindow::createProjectTree()
{
    auto* tree = new QTreeWidget();
    tree->setHeaderLabel("Файлы проекта");

    // Пример структуры
    auto* root = new QTreeWidgetItem(tree);
    root->setText(0, "src/");

    auto* file1 = new QTreeWidgetItem(root);
    file1->setText(0, "main.lisp");

    auto* file2 = new QTreeWidgetItem(root);
    file2->setText(0, "utils.lisp");

    tree->expandAll();

    // Двойной клик для открытия
    connect(tree, &QTreeWidget::itemDoubleClicked, [this](QTreeWidgetItem* item) {
        if (item->childCount() == 0) {
            // Открытие файла...
        }
        });

    return tree;
}

QTextEdit* MainWindow::createREPLConsole()
{
    auto* console = new QTextEdit();
    console->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    console->setMaximumHeight(200);
    console->setPlaceholderText("Lisp REPL ready...");
    return console;
}

QListWidget* MainWindow::createSymbolTable()
{
    auto* list = new QListWidget();
    list->addItem("defun");
    list->addItem("defvar");
    list->addItem("defparameter");
    list->addItem("lambda");
    list->addItem("let");
    list->addItem("if");
    list->addItem("cond");

    return list;
}

void MainWindow::setupMenuBar()
{
    // File menu
    auto* fileMenu = menuBar()->addMenu(tr("&Файл"));
    fileMenu->addAction(tr("Новый"), QKeySequence::New);
    fileMenu->addAction(tr("Открыть..."), QKeySequence::Open);
    fileMenu->addAction(tr("Сохранить"), QKeySequence::Save);
    fileMenu->addAction(tr("Сохранить как..."), QKeySequence::SaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Выйти"), [this]() { close(); });

    // Edit menu
    auto* editMenu = menuBar()->addMenu("&Правка");
    editMenu->addAction("Назад", QKeySequence::Undo);
    editMenu->addAction("Вернуть", QKeySequence::Redo);
    editMenu->addSeparator();
    editMenu->addAction("Вырезать", QKeySequence::Cut);
    editMenu->addAction("Скопировать", QKeySequence::Copy);
    editMenu->addAction("Вырезать", QKeySequence::Paste);

    // Run menu
    auto* runMenu = menuBar()->addMenu("&Запуск");
    runMenu->addSeparator();
    runMenu->addAction("Запустить REPL");
    runMenu->addAction("Очистить REPL");

    // Tools menu
    auto* toolsMenu = menuBar()->addMenu("&Настройки");
    toolsMenu->addAction("Стиль...");
}

void MainWindow::setupStatusBar()
{
    statusBar()->addPermanentWidget(new QLabel("Lisp"));
    statusBar()->addPermanentWidget(new QLabel("Line: 1, Col: 1"));
    statusBar()->showMessage("Ready");
}

void MainWindow::saveLayout()
{
    QSettings settings("LIDE", "LIDE");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("state", saveState());
}

void MainWindow::loadLayout()
{
    QSettings settings("LIDE", "LIDE");
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
        restoreState(settings.value("state").toByteArray());
    }
}

void MainWindow::onDockLocationChanged(Qt::DockWidgetArea area)
{
    saveLayout();
}

void MainWindow::toggleDockWidget(QDockWidget* dock)
{
    dock->setVisible(!dock->isVisible());
}