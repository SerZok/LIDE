#include "mainwindow.h"
#include "ui/ui_mainwindow.h"
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

MainWindow::MainWindow(QWidget* parent): QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("LIDE - Lisp IDE");
    resize(1200, 800);

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
        qDebug() << "Failed to load stylesheet";
    }
}

void MainWindow::setupDockWidgets()
{
    // 1. Центральный редактор (не док, а центральный виджет)
    auto* centralEditor = createLispEditor();
    setCentralWidget(centralEditor);

    // 2. Дерево проекта (слева)
    createDockWidget("Project Browser",
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

    // Сохраняем для меню View
    m_dockNames[dock] = title;

    // Подключаем сигнал изменения позиции
    connect(dock, &QDockWidget::dockLocationChanged,
        this, &MainWindow::onDockLocationChanged);
}

QTextEdit* MainWindow::createLispEditor()
{
    auto* editor = new QTextEdit();
    editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    editor->setFontPointSize(11);
    editor->setPlaceholderText(";; Enter Lisp code here...");
    return editor;
}

QTreeWidget* MainWindow::createProjectTree()
{
    auto* tree = new QTreeWidget();
    tree->setHeaderLabel("Project Files");

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
        if (item->childCount() == 0) { // файл, не папка
            QMessageBox::information(this, QObject::tr("Открыть"), "Opening: " + item->text(0));
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
    auto* runMenu = menuBar()->addMenu("&Run");
    runMenu->addAction("Evaluate Buffer", QKeySequence("Ctrl+E"));
    runMenu->addAction("Evaluate Region", QKeySequence("Ctrl+R"));
    runMenu->addSeparator();
    runMenu->addAction("Запустить REPL");
    runMenu->addAction("Очистить REPL");

    // Tools menu
    auto* toolsMenu = menuBar()->addMenu("&Tools");
    toolsMenu->addAction("Preferences...");
    toolsMenu->addAction("Package Manager");
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
        statusBar()->showMessage("Layout restored", 2000);
    }
}

void MainWindow::onDockLocationChanged(Qt::DockWidgetArea area)
{
    // Можно автоматически сохранять или обновлять UI
}

void MainWindow::toggleDockWidget(QDockWidget* dock)
{
    dock->setVisible(!dock->isVisible());
}