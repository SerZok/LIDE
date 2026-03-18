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
#include "QToolBar"

#include "mainwindow.h"
#include "ui/ui_mainwindow.h"

MainWindow::MainWindow(QWidget* parent) :
      QMainWindow(parent)
    ,ui(new Ui::MainWindow)
    ,m_projectTree(nullptr)
    ,m_tabWidget(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("LIDE - LISP IDE");
    resize(1366, 1080);

    setupDockWidgets();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();

    loadAppState();
}

MainWindow::~MainWindow()
{
    saveAppState();
    delete ui;
}

void MainWindow::openFiles(QStringList files) {
    if (!m_tabWidget) return;

    for (const QString& file : files) {
        if (file.endsWith(".lisp", Qt::CaseInsensitive) ||
            file.endsWith(".lsp", Qt::CaseInsensitive)) {
            m_tabWidget->openFile(file);
        }
    }
}

void MainWindow::setupMenuBar()
{
    // File menu
    auto* fileMenu = menuBar()->addMenu(tr("&Файл"));

    m_createFileAction = fileMenu->addAction(QIcon(":/icons/images/new-file.svg"), tr("Новый файл..."), QKeySequence::New);
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

    m_openFileAction = fileMenu->addAction(QIcon(":/icons/images/open-file.svg"), tr("Открыть файл..."), QKeySequence::Open);
    connect(m_openFileAction, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Открытие файла"), m_projectTree->rootPath(), tr("Lisp файлы (* .lisp *.lsp * .asd)"));
        if (!path.isEmpty()) m_tabWidget->openFile(path);
        });
    addAction(m_openFileAction);

    fileMenu->addSeparator();

    m_openProjectAction = fileMenu->addAction(QIcon(":/icons/images/open-project.svg"), tr("Открыть проект..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    m_newProjectAction = fileMenu->addAction(tr("Новый проект..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    connect(m_openProjectAction, &QAction::triggered, this, &MainWindow::openProject);
    connect(m_newProjectAction, &QAction::triggered, this, &MainWindow::createProject);

    fileMenu->addSeparator();

    m_saveFileAction = fileMenu->addAction(QIcon(":/icons/images/save.svg"), tr("Сохранить"), QKeySequence::Save);
    m_saveFileAllAction = fileMenu->addAction(QIcon(":/icons/images/save-all.svg"), tr("Сохранить все"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    m_saveFileAsAction = fileMenu->addAction(tr("Сохранить как..."), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));

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

    connect(m_saveFileAllAction, &QAction::triggered, [this]() {
        m_tabWidget->saveAll();
        });

    fileMenu->addSeparator();
    fileMenu->addAction(tr("Выйти"), [this]() { close(); });

    // Edit menu
    auto* editMenu = menuBar()->addMenu(tr("&Правка"));
    m_redoAction = editMenu->addAction(QIcon(":/icons/images/redo.svg"),tr("Повтор"), QKeySequence::Redo);
    m_undoAction = editMenu->addAction(QIcon(":/icons/images/undo.svg"), tr("Отмена"), QKeySequence::Undo);
    editMenu->addSeparator();
    m_copyAction = editMenu->addAction(QIcon(":/icons/images/copy.svg"), tr("Скопировать"), QKeySequence::Copy);
    m_cutAction = editMenu->addAction(QIcon(":/icons/images/cut.svg"), tr("Вырезать"), QKeySequence::Cut);
    m_pasteAction = editMenu->addAction(QIcon(":/icons/images/paste.svg"), tr("Вставить"), QKeySequence::Paste);

    m_undoAction->setEnabled(false);
    m_redoAction->setEnabled(false);
    m_cutAction->setEnabled(false);
    m_copyAction->setEnabled(false);
    m_pasteAction->setEnabled(false);

    connect(m_tabWidget, &EditorTabWidget::currentEditorChanged, this, [this](LispEditor* editor) {
        if (editor) {
            connect(m_undoAction, &QAction::triggered, editor, &LispEditor::undo, Qt::UniqueConnection);
            connect(m_redoAction, &QAction::triggered, editor, &LispEditor::redo, Qt::UniqueConnection);
            connect(m_cutAction, &QAction::triggered, editor, &LispEditor::cut, Qt::UniqueConnection);
            connect(m_copyAction, &QAction::triggered, editor, &LispEditor::copy, Qt::UniqueConnection);
            connect(m_pasteAction, &QAction::triggered, editor, &LispEditor::paste, Qt::UniqueConnection);

            m_undoAction->setEnabled(editor->document()->isUndoAvailable());
            m_redoAction->setEnabled(editor->document()->isRedoAvailable());
            m_cutAction->setEnabled(true);
            m_copyAction->setEnabled(true);
            m_pasteAction->setEnabled(true);

            // обновления состояния undo/redo
            connect(editor->document(), &QTextDocument::undoAvailable, m_undoAction, &QAction::setEnabled, Qt::UniqueConnection);
            connect(editor->document(), &QTextDocument::redoAvailable, m_redoAction, &QAction::setEnabled, Qt::UniqueConnection);
        }
        else {
            m_undoAction->setEnabled(false);
            m_redoAction->setEnabled(false);
            m_cutAction->setEnabled(false);
            m_copyAction->setEnabled(false);
            m_pasteAction->setEnabled(false);
        }
        });

    // View
    auto* viewMenu = menuBar()->addMenu(tr("&Вид"));
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
    auto* runMenu = menuBar()->addMenu(tr("&Запуск"));
    runMenu->addSeparator();
    m_startReplAction = runMenu->addAction(QIcon(":/icons/images/start.svg"), tr("Запустить REPL"));

    runMenu->addAction(tr("Очистить REPL"));

    // Tools menu
    auto* toolsMenu = menuBar()->addMenu(tr("&Настройки"));
    auto styleMenu = toolsMenu->addMenu(tr("Тема..."));

    // Создаём группу для взаимного исключения
    auto* themeGroup = new QActionGroup(this);

    m_lightStyleAction = styleMenu->addAction(QIcon(":/icons/images/light-theme.svg"), tr("Светлая"));
    m_lightStyleAction->setActionGroup(themeGroup);

    m_darkStyleAction = styleMenu->addAction(QIcon(":/icons/images/dark-theme.svg"), tr("Тёмная"));
    m_darkStyleAction->setActionGroup(themeGroup);

    connect(themeGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        QString theme = (action == m_lightStyleAction) ? "light" : "dark";
        loadTheme(theme);
        });

    // Справка
    auto* helpMenu = menuBar()->addMenu(tr("&Справка"));
    auto aboutAction = helpMenu->addAction(QIcon(":/icons/images/info.svg"), tr("О программе"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
        });

}

void MainWindow::setupToolBar() {
    m_mainToolBar = addToolBar(tr("Панель инструментов"));
    m_mainToolBar->setMovable(false);
    m_mainToolBar->setIconSize(QSize(24, 24));

    m_mainToolBar->addAction(m_openProjectAction);
    m_mainToolBar->addAction(m_createFileAction);
    m_mainToolBar->addAction(m_openFileAction);
    m_mainToolBar->addAction(m_saveFileAction);
    m_mainToolBar->addAction(m_saveFileAllAction);
    m_mainToolBar->addAction(m_startReplAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_startReplAction);
}

void MainWindow::setupStatusBar()
{
    statusBar()->addPermanentWidget(new QLabel("Lisp"));
    statusBar()->addPermanentWidget(new QLabel("Line: 1, Col: 1"));
    statusBar()->showMessage("Ready");
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
    m_dockNames[m_projectDock] = m_projectDock->windowTitle();
    addDockWidget(Qt::LeftDockWidgetArea, m_projectDock);

    m_ConsoleDock = new QDockWidget(tr("Консоль LISP"), this);
    m_ConsoleDock->setWidget(createConsoleLisp());
    m_ConsoleDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_ConsoleDock->setObjectName("console_dock");
    m_dockNames[m_ConsoleDock] = m_ConsoleDock->windowTitle();
    addDockWidget(Qt::BottomDockWidgetArea, m_ConsoleDock);

    QTimer::singleShot(0, [this]() {
        if (m_projectTree && m_projectDock) {
            QString currentPath = m_projectTree->rootPath();
            if (!currentPath.isEmpty()) {
                QFileInfo info(currentPath);
                m_projectDock->setWindowTitle(tr("Дерево проекта - %1").arg(info.fileName()));
                m_projectDock->setToolTip(tr("Текущий проект: %1").arg(currentPath));
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
    connect(this, &MainWindow::themeChanged, Settings::instance(), &Settings::setCurrentTheme);
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

void MainWindow::loadTheme(QString theme)
{
    QFile file(QString(":styles/%1.css").arg(theme));
    if (file.open(QFile::ReadOnly)) {
        auto* settings = Settings::instance();
        settings->setCurrentTheme(theme);

        QString styleSheet = QLatin1String(file.readAll());
        qApp->setStyleSheet(styleSheet);
        file.close();

        emit themeChanged(theme);
    }
    else {
        qDebug() << "Ошибка загрузки темы:" << theme;
    }
}

void MainWindow::openProject() {
    QString path = QFileDialog::getExistingDirectory(this, tr("Открыть проект"), m_projectTree->rootPath(), QFileDialog::ReadOnly | QFileDialog::ShowDirsOnly);
    if (!path.isEmpty()) {
        m_projectTree->openProject(path);
    }
}

void MainWindow::createProject() {
    QString parentPath = QFileDialog::getExistingDirectory(
        this,
        tr("Выберите папку для нового проекта"),
        m_projectTree->rootPath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (parentPath.isEmpty()) return;

    // ИМЯ проекта(папки)
    QString projectName = QInputDialog::getText(this, tr("Новый проект"), tr("Название проекта:")
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