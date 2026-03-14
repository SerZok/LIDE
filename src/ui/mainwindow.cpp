#include <QTextEdit>
#include <QTreeWidget>
#include <QListWidget>
#include <QDockWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QSplitter>
#include <QLabel>
#include <QFontDatabase>
#include <QMessageBox>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLayout>

#include "mainwindow.h"
#include "ui/ui_mainwindow.h"

MainWindow::MainWindow(QWidget* parent) :
      QMainWindow(parent)
    ,ui(new Ui::MainWindow)
    ,m_projectTree(nullptr)
    ,m_tabWidget(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("LIDE - Lisp IDE");
    resize(1366, 1080);

    setupDockWidgets();
    setupMenuBar();
    setupStatusBar();

    loadAppState();
}

MainWindow::~MainWindow()
{
    saveAppState();
    delete ui;
}

void MainWindow::loadTheme(QString theme)
{
    QFile file(QString(":styles/%1.css").arg(theme));
    if (file.open(QFile::ReadOnly)) {
        auto* settings = Settings::instance();
        settings->setCurrentTheme(theme);

        QString styleSheet = QLatin1String(file.readAll());
        qApp->setStyleSheet(styleSheet);
        qDebug() << "Стиль: " << file.fileName() << " загружен успешно!";
        file.close();

        emit themeChanged(theme);
    }
    else {
        qDebug() << "Ошибка загрузки темы:" << theme;
    }
}

void MainWindow::openFiles(QStringList files) {
    if (!m_tabWidget) return;

    for (const auto& file : files) {
        m_tabWidget->openFile(file);
    }
}

void MainWindow::setupDockWidgets()
{
    m_tabWidget = new EditorTabWidget(this);
    m_tabWidget->setObjectName("editorTabWidget");
    setCentralWidget(m_tabWidget);

    m_projectDock = new QDockWidget(tr("Дерево проекта"), this);
    m_projectDock->setWidget(createProjectTree());
    m_projectDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_projectDock->setObjectName("treeProject_dock");
    addDockWidget(Qt::LeftDockWidgetArea, m_projectDock);

    m_ConsoleDock = new QDockWidget(tr("Консоль LISP"), this);
    m_ConsoleDock->setWidget(createConsoleLisp());
    m_ConsoleDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_ConsoleDock->setObjectName("console_dock");
    addDockWidget(Qt::BottomDockWidgetArea, m_ConsoleDock);

    QTimer::singleShot(0, [this]() {
        if (m_projectTree && m_projectDock) {
            QString currentPath = m_projectTree->rootPath();
            if (!currentPath.isEmpty()) {
                QFileInfo info(currentPath);
                m_projectDock->setWindowTitle(tr("Дерево проекта - %1").arg(info.fileName()));
                m_projectDock->setToolTip(tr("Текущий проект: %1").arg(info.absolutePath()));
            }
        }
        });

    setupConnections();
}

void MainWindow::setupConnections() {
    connect(m_tabWidget, &EditorTabWidget::currentFileChanged, m_projectTree, &ProjectTree::onFileLoaded);
    connect(m_tabWidget, &EditorTabWidget::fileModifiedChanged, m_projectTree, &ProjectTree::onFileModifiedChanged);
    connect(m_tabWidget, &EditorTabWidget::fileClosed, m_projectTree, &ProjectTree::onFileClosed);
    connect(m_projectTree, &ProjectTree::fileActivated, m_tabWidget, &EditorTabWidget::openFile);
    connect(m_projectTree, &ProjectTree::projectOpened, this, [this](const QString& projectPath) {
            QFileInfo info(projectPath);
            if (!info.exists() || !info.isDir()) return;

            m_projectDock->setWindowTitle(tr("Дерево проекта - %1").arg(info.fileName()));
            m_projectDock->setToolTip(tr("Текущий проект: %1").arg(info.absolutePath()));
        });
    connect(this, &MainWindow::themeChanged, m_tabWidget, &EditorTabWidget::onThemeChanged);
}


ProjectTree* MainWindow::createProjectTree()
{
    m_projectTree = new ProjectTree();
    m_projectTree->setObjectName("projectTree_1");
    return m_projectTree;
}

Console* MainWindow::createConsoleLisp()
{
    m_console = new Console();
    m_console->setObjectName("consoleLisp_1");
    return m_console;
}

void MainWindow::setupMenuBar()
{
    // File menu
    auto* fileMenu = menuBar()->addMenu(tr("&Файл"));

    m_createFileAction = fileMenu->addAction(tr("Новый файл..."), QKeySequence::New);
    connect(m_createFileAction, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, tr("Новый файл"), m_projectTree->rootPath(), tr("Lisp файлы (*.lisp *.lsp *.asd)"));
        if (!path.isEmpty()) {
            if (QFile::exists(path)) {
                int ret = QMessageBox::question(this, tr("Файл существует"), tr("Файл %1 уже существует.\nПерезаписать?").arg(path), QMessageBox::Yes | QMessageBox::No);
                if (ret != QMessageBox::Yes) {
                    return;
                }
            }

            QFile file(path);
            if (file.open(QIODevice::WriteOnly)) {
                file.close();
                m_tabWidget->openFile(path);
            }
            else {
                QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось создать файл %1").arg(path));
            }
        }
        });
    addAction(m_createFileAction);

    m_openFileAction = fileMenu->addAction(tr("Открыть файл..."), QKeySequence::Open);
    connect(m_openFileAction, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Открытие файла"), m_projectTree->rootPath(), tr("Lisp файлы (* .lisp *.lsp * .asd)"));
        if (!path.isEmpty()) m_tabWidget->openFile(path);
        });
    addAction(m_openFileAction);

    fileMenu->addSeparator();

    m_openProjectAction = fileMenu->addAction(tr("Открыть проект..."));
    m_openProjectAction->setShortcut(QKeySequence((Qt::CTRL | Qt::SHIFT | Qt::Key_O)));
    m_newProjectAction = fileMenu->addAction(tr("Новый проект..."));
    m_newProjectAction->setShortcut(QKeySequence((Qt::CTRL | Qt::SHIFT | Qt::Key_N)));
    connect(m_openProjectAction, &QAction::triggered, this, &MainWindow::openProject);
    connect(m_newProjectAction, &QAction::triggered, this, &MainWindow::createProject);

    fileMenu->addSeparator();

    m_saveFileAction = fileMenu->addAction(tr("Сохранить"), QKeySequence::Save);
    auto m_saveFileAsAction = fileMenu->addAction(tr("Сохранить как..."), QKeySequence::SaveAs);

    connect(m_saveFileAction, &QAction::triggered, [this]() {
        if (auto* editor = m_tabWidget->currentEditor()) {
            editor->saveFile();
        }
        });

    connect(m_saveFileAsAction, &QAction::triggered, [this]() {
        auto* editor = m_tabWidget->currentEditor();
        if (!editor) return;

        QString path = QFileDialog::getSaveFileName(this,
            tr("Сохранение файла как..."),
            m_projectTree->rootPath(),
            tr("Lisp файлы (*.lisp *.lsp *.asd)"));

        if (!path.isEmpty()) {
            editor->saveFileAs(path);
        }
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

        connect(dock, &QDockWidget::visibilityChanged, [this, action](bool visible) {
            // Блокируем сигналы action, чтобы не создавать цикл
            action->blockSignals(true);
            action->setChecked(visible);
            action->blockSignals(false);
            });

        connect(action, &QAction::toggled, [this, dock](bool checked) {
            dock->setVisible(checked);
            });
    }

    // Run menu
    auto* runMenu = menuBar()->addMenu("&Запуск");
    runMenu->addSeparator();
    runMenu->addAction("Запустить REPL");
    runMenu->addAction("Очистить REPL");

    // Tools menu
    auto* toolsMenu = menuBar()->addMenu("&Настройки");
    auto styleMenu = toolsMenu->addMenu("Тема...");

    // Создаём группу для взаимного исключения
    auto* themeGroup = new QActionGroup(this);

    m_lightStyleAction = styleMenu->addAction("Светлая");
    m_lightStyleAction->setCheckable(true);
    m_lightStyleAction->setActionGroup(themeGroup);

    m_darkStyleAction = styleMenu->addAction("Тёмная");
    m_darkStyleAction->setCheckable(true);
    m_darkStyleAction->setActionGroup(themeGroup);

    QString currentTheme = Settings::instance()->currentTheme();
    m_lightStyleAction->setChecked(currentTheme == "light");
    m_darkStyleAction->setChecked(currentTheme == "dark");

    connect(themeGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        QString theme = (action == m_lightStyleAction) ? "light" : "dark";
        loadTheme(theme);
        });
}

void MainWindow::setupStatusBar()
{
    statusBar()->addPermanentWidget(new QLabel("Lisp"));
    statusBar()->addPermanentWidget(new QLabel("Line: 1, Col: 1"));
    statusBar()->showMessage("Ready");
}

void MainWindow::saveAppState()
{
    auto* settings = Settings::instance();

    QByteArray state = saveState();
    settings->setMainWindowState(state);
    settings->setMainWindowGeometry(saveGeometry());

    if(m_tabWidget)
        settings->setOpenedFiles(m_tabWidget->openedFiles());
}

void MainWindow::loadAppState()
{
    auto* settings = Settings::instance();

    restoreGeometry(settings->mainWindowGeometry());
    restoreState(settings->mainWindowState());
    loadTheme(settings->currentTheme());
    openFiles(settings->openedFiles());
    
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