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
#include <QFileDialog>
#include <QInputDialog>

#include "mainwindow.h"
#include "ui/ui_mainwindow.h"

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_lispEditor(nullptr),
    m_projectTree(nullptr)
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

void MainWindow::loadStyleSheet() 
{
    QFile file(":/styles/dark.css");
    if (file.open(QFile::ReadOnly)) {
        qDebug() << "Загрузка файл стиля: " << file.fileName();
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
    m_lispEditor = new LispEditor();
    setCentralWidget(m_lispEditor);

    // 2. Дерево проекта (слева)
    createDockWidget(tr("Дерево проекта"),
        createProjectTree(),
        Qt::LeftDockWidgetArea);

    // 3. REPL консоль (снизу)
    createDockWidget(tr("Консоль REPL"),
        createREPLConsole(),
        Qt::BottomDockWidgetArea);

    // 4. Список символов/переменных (справа)
    //createDockWidget(tr("Список переменных"),
    //    createSymbolTable(),
    //    Qt::RightDockWidgetArea);

    setupConnections();
}

void MainWindow::setupConnections() {
    connect(m_projectTree, &ProjectTree::fileActivated, m_lispEditor, &LispEditor::loadFile);
    connect(m_lispEditor, &LispEditor::fileLoaded, m_projectTree, &ProjectTree::onFileLoaded);
    connect(m_lispEditor, &LispEditor::fileSaved, m_projectTree, &ProjectTree::onFileSaved);
    connect(m_lispEditor, &LispEditor::fileModifiedChanged, m_projectTree, &ProjectTree::onFileModifiedChanged);
    connect(m_lispEditor, &LispEditor::fileClosed, m_projectTree, &ProjectTree::onFileClosed);
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

ProjectTree* MainWindow::createProjectTree()
{
    m_projectTree = new ProjectTree();
    return m_projectTree;
}

Console* MainWindow::createREPLConsole()
{
    m_console = new Console();
    return m_console;
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

    m_openFileAction = fileMenu->addAction(tr("Открыть..."), QKeySequence::Open);
    connect(m_openFileAction, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Открытие файла"), m_projectTree->rootPath(), tr("Lisp файлы (* .lisp *.lsp * .asd)"));
        if (!path.isEmpty()) m_lispEditor->loadFile(path);
        });

    fileMenu->addSeparator();

    m_openProjectAction = fileMenu->addAction(tr("Открыть проект..."), QKeySequence::Open);
    m_newProjectAction = fileMenu->addAction(tr("Новый проект..."));
    connect(m_openProjectAction, &QAction::triggered, this, &MainWindow::openProject);
    connect(m_newProjectAction, &QAction::triggered, this, &MainWindow::createProject);

    fileMenu->addSeparator();

    m_saveFileAction = fileMenu->addAction(tr("Сохранить"), QKeySequence::Save);
    auto m_saveFileAsAction = fileMenu->addAction(tr("Сохранить как..."), QKeySequence::SaveAs);

    connect(m_saveFileAction, &QAction::triggered, m_lispEditor, &LispEditor::saveFile);
    connect(m_saveFileAsAction, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, tr("Сохранение файла как..."), m_projectTree->rootPath(), tr("Lisp файлы (* .lisp *.lsp * .asd)"));
        if (!path.isEmpty())
            m_lispEditor->saveFileAs(path);
        });

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

    // View
    auto* viewMenu = menuBar()->addMenu("&Вид");
    for (auto it = m_dockNames.begin(); it != m_dockNames.end(); ++it) {
        auto* dock = it.key();
        auto* action = viewMenu->addAction(it.value());
        action->setCheckable(true);
        action->setChecked(dock->isVisible());

        connect(action, &QAction::toggled, [this, dock](bool checked) {
            dock->setVisible(checked);
            });

        connect(dock, &QDockWidget::visibilityChanged, action, &QAction::setChecked);
    }

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

void MainWindow::openProject() {
    QString path = QFileDialog::getExistingDirectory(this, tr("Открыть проект"), m_projectTree->rootPath(), QFileDialog::ReadOnly | QFileDialog::ShowDirsOnly);
    if (!path.isEmpty()) {
        m_projectTree->openProject(path);
    }
}

void MainWindow::createProject() {
    // Выбираем где создавать(папку)
    QString parentPath = QFileDialog::getExistingDirectory(
        this,
        tr("Выберите папку для нового проекта"),
        m_projectTree->rootPath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (parentPath.isEmpty()) return;

    // ИМЯ проекта(папки)
    QString projectName = QInputDialog::getText(
        this,
        tr("Новый проект"),
        tr("Название проекта:")
    );

    if (!projectName.isEmpty()) {
        QString newPath = parentPath + "/" + projectName;
        QDir dir;
        if (dir.mkpath(newPath)) {
            m_projectTree->openProject(newPath);
        }
        else {
            QMessageBox::critical(this, tr("Ошибка"), tr("Не удалось создать проект"));
        }
    }
}