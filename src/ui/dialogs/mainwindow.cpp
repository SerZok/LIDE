#include "mainwindow.h"
#include "ui/ui_mainwindow.h"

#include <QMenu>
#include <QFile>
#include <QLabel>
#include <QAction>
#include <QLayout>
#include <QToolBar>
#include <QMenuBar>
#include <QSplitter>
#include <QTextEdit>
#include <QTreeWidget>
#include <QListWidget>
#include <QDockWidget>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QActionGroup>
#include <QFontDatabase>

MainWindow::MainWindow(QWidget* parent) :
      QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_projectTree(nullptr)
    , m_tabWidget(nullptr)
    , m_settings(Settings::instance())
{
    ui->setupUi(this);
    resize(1366, 1080);
    connect(m_settings, &Settings::settingsChanged, this, &MainWindow::onSettingsChanged);

    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(false);
    m_autoSaveTimer->setTimerType(Qt::VeryCoarseTimer);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::triggerAutoSave);

    setupSaveTimer();
    setupDockWidgets();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();

    loadAppState();
}

MainWindow::~MainWindow()
{
    if (m_tabWidget) {
        if (m_settings->saveExit())
            m_tabWidget->saveAll();
        else
        {
            // Спрашиваем, надо ли сохранять ?
            if (int modCnt = m_tabWidget->modifFileCounts()) {
                int ret = QMessageBox::question(this,
                    tr("LIDE"),
                    tr("Есть несохраненные файлы: %1. Сохранить?").arg(modCnt),
                    QMessageBox::Yes | QMessageBox::No);

                if (ret == QMessageBox::Yes) {
                    m_tabWidget->saveAll();;
                }
            }
        }
    }
    saveAppState();
    delete ui;
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        updateTranslations();
    }
    QMainWindow::changeEvent(event);
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

void MainWindow::setupSaveTimer() {
    int intervalMinutes = m_settings->saveTime();
    int intervalMs = intervalMinutes * 60 * 1000;

    if (intervalMs > 0 && m_tabWidget) {
        m_autoSaveTimer->setInterval(intervalMs);
        m_autoSaveTimer->start();
    }
    else {
        m_autoSaveTimer->stop();
    }
}

void MainWindow::triggerAutoSave() {
    if (!m_tabWidget) return;

    m_tabWidget->saveAll();
}

void MainWindow::onSettingsChanged() {
    setupSaveTimer();
}

void MainWindow::setupMenuBar()
{
    // File menu
    m_fileMenu = menuBar()->addMenu(tr("&Файл"));

    m_createFileAction = m_fileMenu->addAction(QIcon(":/icons/images/new-file.svg"), tr("Новый файл..."), QKeySequence::New);
    connect(m_createFileAction, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, tr("Новый файл"), m_projectTree->rootPath(), tr("Lisp файлы (*.lisp *.lsp)"));
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

    m_openFileAction = m_fileMenu->addAction(QIcon(":/icons/images/open-file.svg"), tr("Открыть файл..."), QKeySequence::Open);
    connect(m_openFileAction, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Открытие файла"), m_projectTree->rootPath(), tr("Lisp файлы (* .lisp *.lsp)"));
        if (!path.isEmpty()) m_tabWidget->openFile(path);
        });
    addAction(m_openFileAction);

    m_fileMenu->addSeparator();

    m_openProjectAction = m_fileMenu->addAction(QIcon(":/icons/images/open-project.svg"), tr("Открыть проект..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    m_newProjectAction = m_fileMenu->addAction(tr("Новый проект..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    connect(m_openProjectAction, &QAction::triggered, this, &MainWindow::openProject);
    connect(m_newProjectAction, &QAction::triggered, this, &MainWindow::createProject);

    m_fileMenu->addSeparator();

    m_saveFileAction = m_fileMenu->addAction(QIcon(":/icons/images/save.svg"), tr("Сохранить"), QKeySequence::Save);
    m_saveFileAllAction = m_fileMenu->addAction(QIcon(":/icons/images/save-all.svg"), tr("Сохранить все"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    m_saveFileAsAction = m_fileMenu->addAction(tr("Сохранить как..."), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));

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

    auto* exitAction = m_fileMenu->addAction(tr("Выйти"), [this]() { close(); });
    exitAction->setObjectName("actionExit");

    // Edit menu
    m_editMenu = menuBar()->addMenu(tr("&Правка"));
    m_redoAction = m_editMenu->addAction(QIcon(":/icons/images/redo.svg"),tr("Повтор"), QKeySequence::Redo);
    m_undoAction = m_editMenu->addAction(QIcon(":/icons/images/undo.svg"), tr("Отмена"), QKeySequence::Undo);
    m_editMenu->addSeparator();
    m_copyAction = m_editMenu->addAction(QIcon(":/icons/images/copy.svg"), tr("Скопировать"), QKeySequence::Copy);
    m_cutAction = m_editMenu->addAction(QIcon(":/icons/images/cut.svg"), tr("Вырезать"), QKeySequence::Cut);
    m_pasteAction = m_editMenu->addAction(QIcon(":/icons/images/paste.svg"), tr("Вставить"), QKeySequence::Paste);

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

            m_runAction->setEnabled(true);
            m_restartAction->setEnabled(true);
            m_cleanRunAction->setEnabled(true);

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
            m_runAction->setEnabled(false);
            m_restartAction->setEnabled(false);
            m_cleanRunAction->setEnabled(false);
        }
        });

    // View
    m_viewMenu = menuBar()->addMenu(tr("&Вид"));
    auto* fullScreenAction = new QAction(tr("Полноэкранный режим"), this);
    fullScreenAction->setObjectName("actionFullScreen");
    fullScreenAction->setShortcut(QKeySequence::FullScreen);
    fullScreenAction->setCheckable(true);
    fullScreenAction->setChecked(isFullScreen());
    connect(fullScreenAction, &QAction::triggered, this, [this](bool checked) {
        if (checked)
            showFullScreen();
        else
            showNormal();
        });
    addAction(fullScreenAction);
    m_viewMenu->addAction(fullScreenAction);
    m_viewMenu->addSeparator();
    for (auto it = m_dockNames.begin(); it != m_dockNames.end(); ++it) {
        auto* dock = it.key();
        auto* action = m_viewMenu->addAction(it.value());
        action->setCheckable(true);
        action->setChecked(dock->isVisible());

        connect(dock, &QDockWidget::visibilityChanged, [this, action](bool visible) {
            action->blockSignals(true);
            action->setChecked(visible);
            action->blockSignals(false);
            });

        connect(action, &QAction::toggled, [this, dock](bool checked) {
            dock->setVisible(checked);
            });
    }

    // Run menu
    m_runMenu = menuBar()->addMenu(tr("&Запуск"));
    m_runMenu->addSeparator();
    m_runAction = m_runMenu->addAction(QIcon(":/icons/images/start.svg"), tr("Запустить"), QKeySequence(Qt::Key_F5));
    m_runAction->setEnabled(false);
    m_runAction->setToolTip(tr("Запустить текущий код в REPL"));
    connect(m_runAction, &QAction::triggered, this, [this]() {
        if (m_console && m_tabWidget) {
            QString filePath = m_tabWidget->currentFilePath();
            if (!filePath.isEmpty()) {
                m_tabWidget->currentEditor()->saveFile();
                m_tabWidget->currentEditor()->clearErrorHighlight();
                m_console->sendFile(filePath);
            }
        }
        });

    m_restartAction = m_runMenu->addAction(QIcon(":/icons/images/restart.svg"), tr("Перезапустить SBCL"), QKeySequence(Qt::CTRL | Qt::Key_R));
    m_restartAction->setEnabled(false);
    m_restartAction->setToolTip(tr("Перезапустить SBCL"));
    connect(m_restartAction, &QAction::triggered, this, [this]() {
        if (m_console){
            m_console->restart();
            }
        });

    m_cleanRunAction = m_runMenu->addAction(QIcon(":/icons/images/force-start.svg"), tr("Принудительный запуск"), QKeySequence(Qt::CTRL | Qt::Key_F5));
    m_cleanRunAction->setEnabled(false);
    m_cleanRunAction->setToolTip(tr("Перезапуск SBCL и запуск текущего кода"));
    connect(m_cleanRunAction, &QAction::triggered, this, [this]() {
        if (m_console && m_tabWidget) {
            QString filePath = m_tabWidget->currentFilePath();
            if (!filePath.isEmpty()) {
                m_tabWidget->currentEditor()->saveFile();
                m_tabWidget->currentEditor()->clearErrorHighlight();
                m_console->restart();
                m_console->sendFile(filePath);
            }
        }
        });

    // Tools menu
    m_toolsMenu = menuBar()->addMenu(tr("&Опции"));

    // > Настройки
    auto settingsAction = m_toolsMenu->addAction(tr("Настройки"));
    settingsAction->setObjectName("actionSettings");
    connect(settingsAction, &QAction::triggered, this, [this]() {
        SettingsDialog dlg(this);
        dlg.exec();
        });

    //  > Тема
    m_styleMenu = m_toolsMenu->addMenu(tr("Тема"));
    auto* themeGroup = new QActionGroup(this);
    m_lightStyleAction = m_styleMenu->addAction(QIcon(":/icons/images/light-theme.svg"), tr("Светлая"));
    m_lightStyleAction->setActionGroup(themeGroup);
    m_darkStyleAction = m_styleMenu->addAction(QIcon(":/icons/images/dark-theme.svg"), tr("Тёмная"));
    m_darkStyleAction->setActionGroup(themeGroup);
    connect(themeGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        QString theme = (action == m_lightStyleAction) ? "light" : "dark";
        m_settings->setCurrentTheme(theme);
        });


    // Справка
    m_helpMenu = menuBar()->addMenu(tr("&Справка"));
    auto aboutAction = m_helpMenu->addAction(QIcon(":/icons/images/info.svg"), tr("О программе"));
    aboutAction->setObjectName("actionAbout");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
        });

    // TODO: добавить рукводство пользователя

}

void MainWindow::setupToolBar() {
    m_mainToolBar = addToolBar(tr("Панель инструментов"));
    m_mainToolBar->setObjectName("ToolBar");
    m_mainToolBar->setMovable(false);
    m_mainToolBar->setIconSize(QSize(24, 24));

    m_mainToolBar->addAction(m_openProjectAction);
    m_mainToolBar->addAction(m_createFileAction);
    m_mainToolBar->addAction(m_openFileAction);
    m_mainToolBar->addAction(m_saveFileAction);
    m_mainToolBar->addAction(m_saveFileAllAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_runAction);
    m_mainToolBar->addAction(m_restartAction);
    m_mainToolBar->addAction(m_cleanRunAction);
}

void MainWindow::setupStatusBar()
{
    m_positionLabel = new QLabel("Line: 1, Col: 1");
    statusBar()->addPermanentWidget(m_positionLabel);
}

void MainWindow::updateStatusBarPosition(int line, int col)
{
    m_positionLabel->setText(tr("Line: %1, Col: %2").arg(line).arg(col));
}

void MainWindow::setupDockWidgets()
{
    m_tabWidget = new EditorTabWidget(this);
    m_tabWidget->setObjectName("editorTabWidget");
    setCentralWidget(m_tabWidget);

    m_projectDock = new QDockWidget(tr("Дерево проекта"), this);
    m_dockNames[m_projectDock] = tr("Дерево проекта");
    m_projectDock->setWidget(createProjectTree());
    m_projectDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_projectDock->setObjectName("treeProject_dock");
    addDockWidget(Qt::LeftDockWidgetArea, m_projectDock);

    m_ConsoleDock = new QDockWidget(tr("REPL"), this);
    m_dockNames[m_ConsoleDock] = tr("REPL консоль");
    m_ConsoleDock->setWidget(createConsoleLisp());
    m_ConsoleDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_ConsoleDock->setObjectName("console_dock");
    addDockWidget(Qt::BottomDockWidgetArea, m_ConsoleDock);

    // Подключаем сигнал ошибки из консоли
    //connect(m_console, &ReplWidget::errorOccurred,
    //    this, &MainWindow::onLispError);

    QTimer::singleShot(0, [this]() {
        if (m_projectTree && m_projectDock) {
            QString currentPath = m_projectTree->rootPath();
            if (!currentPath.isEmpty()) {
                QFileInfo info(currentPath);
                m_projectDock->setWindowTitle(tr("%1/").arg(info.fileName()));
                m_projectDock->setToolTip(tr("%1").arg(currentPath));
            }
        }
        });

    connect(m_tabWidget, &EditorTabWidget::currentFileChanged, m_projectTree, &ProjectTree::onFileLoaded);
    connect(m_tabWidget, &EditorTabWidget::fileModifiedChanged, m_projectTree, &ProjectTree::onFileModifiedChanged);
    connect(m_tabWidget, &EditorTabWidget::fileClosed, m_projectTree, &ProjectTree::onFileClosed);
    connect(m_projectTree, &ProjectTree::fileActivated, m_tabWidget, &EditorTabWidget::openFile);
    connect(m_projectTree, &ProjectTree::projectOpened, this, [this](const QString& projectPath) {
        QFileInfo info(projectPath);
        if (!info.exists() || !info.isDir()) return;

        m_projectDock->setWindowTitle(tr("%1/").arg(info.fileName()));
        m_projectDock->setToolTip(tr("%1").arg(info.absolutePath()));
        });

    connect(m_tabWidget, &EditorTabWidget::currentEditorChanged,
        this, [this](LispEditor* editor) {
            if (editor) {
                connect(editor, &LispEditor::cursorPositionChanged,
                    this, [this, editor]() {
                        QTextCursor cursor = editor->textCursor();
                        int line = cursor.blockNumber() + 1;
                        int col = cursor.columnNumber() + 1;
                        updateStatusBarPosition(line, col);
                    });

                // Инициализация текущего редактора
                QTextCursor cursor = editor->textCursor();
                updateStatusBarPosition(cursor.blockNumber() + 1, cursor.columnNumber() + 1);
            }
            else {
                updateStatusBarPosition(0, 0);
            }
        });
}

void MainWindow::onLispError(const QString& file,
    const QString& message, int line, int column)
{
    if (m_tabWidget) {
        LispEditor* editor = findEditorByFile(file);
        if (editor) {
            // Подсвечиваем ошибку по позиции в файле
            editor->highlightErrorAtPosition(message, line, column);

            // Показываем сообщение об ошибке в статус-баре
            statusBar()->showMessage(QString("Ошибка: %1 (строка %2, колонка %3)")
                .arg(message).arg(line).arg(column), 10000);

            // Переключаемся на вкладку
            int index = m_tabWidget->indexOf(editor);
            if (index >= 0) {
                m_tabWidget->setCurrentIndex(index);
            }
        }
    }
}

LispEditor* MainWindow::findEditorByFile(const QString& filePath)
{
    // Нормализуем пути для сравнения
    QString normalizedFile = QFileInfo(filePath).absoluteFilePath();

    for (int i = 0; i < m_tabWidget->count(); ++i) {
        LispEditor* editor = m_tabWidget->editorAt(i);
        if (editor) {
            QString editorFile = editor->currentFile();
            if (QFileInfo(editorFile).absoluteFilePath() == normalizedFile) {
                return editor;
            }
        }
    }
    return nullptr;
}

ProjectTree* MainWindow::createProjectTree()
{
    m_projectTree = new ProjectTree();
    m_projectTree->setObjectName("projectTree_1");
    return m_projectTree;
}

ReplWidget* MainWindow::createConsoleLisp()
{
    m_console = new ReplWidget();
    m_console->setObjectName("consoleLisp_1");
    return m_console;
}

void MainWindow::saveAppState()
{
    QByteArray state = saveState();
    m_settings->setMainWindowState(state);
    m_settings->setMainWindowGeometry(saveGeometry());

    if(m_tabWidget)
        m_settings->setOpenedFiles(m_tabWidget->openedFiles());
}

void MainWindow::loadAppState()
{
    restoreGeometry(m_settings->mainWindowGeometry());
    restoreState(m_settings->mainWindowState());
    openFiles(m_settings->openedFiles());
    
    m_settings->setCurrentTheme(m_settings->currentTheme());
    m_settings->setCurrentLang(m_settings->currentLang());
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

    QString projectName = QInputDialog::getText(
        this,
        tr("Новый проект"),
        tr("Название проекта:"));
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

void MainWindow::updateTranslations()
{
    // ── Заголовки меню ─────────────────────────────────
    if (m_fileMenu) m_fileMenu->setTitle(tr("&Файл"));
    if (m_editMenu) m_editMenu->setTitle(tr("&Правка"));
    if (m_viewMenu) m_viewMenu->setTitle(tr("&Вид"));
    if (m_runMenu) m_runMenu->setTitle(tr("&Запуск"));
    if (m_toolsMenu) m_toolsMenu->setTitle(tr("&Опции"));
    if (m_helpMenu) m_helpMenu->setTitle(tr("&Справка"));
    if (m_styleMenu) m_styleMenu->setTitle(tr("Тема"));


    // ── File Menu Actions ─────────────────────────────
    if (m_createFileAction) m_createFileAction->setText(tr("Новый файл..."));
    if (m_openFileAction) m_openFileAction->setText(tr("Открыть файл..."));
    if (m_openProjectAction) m_openProjectAction->setText(tr("Открыть проект..."));
    if (m_newProjectAction) m_newProjectAction->setText(tr("Новый проект..."));
    if (m_saveFileAction) m_saveFileAction->setText(tr("Сохранить"));
    if (m_saveFileAllAction) m_saveFileAllAction->setText(tr("Сохранить все"));
    if (m_saveFileAsAction) m_saveFileAsAction->setText(tr("Сохранить как..."));

    // ── Edit Menu Actions ──────────────────────────────
    if (m_undoAction) m_undoAction->setText(tr("Отмена"));
    if (m_redoAction) m_redoAction->setText(tr("Повтор"));
    if (m_cutAction) m_cutAction->setText(tr("Вырезать"));
    if (m_copyAction) m_copyAction->setText(tr("Скопировать"));
    if (m_pasteAction) m_pasteAction->setText(tr("Вставить"));

    // ── Run Menu Actions ───────────────────────────────
    if (m_runAction) {
        m_runAction->setText(tr("Запустить"));
        m_runAction->setToolTip(tr("Запустить текущий код в REPL"));
    }
    if (m_restartAction) {
        m_restartAction->setText(tr("Перезапустить SBCL"));
        m_restartAction->setToolTip(tr("Перезапустить SBCL"));
    }
    if (m_cleanRunAction) {
        m_cleanRunAction->setText(tr("Принудительный запуск"));
        m_cleanRunAction->setToolTip(tr("Перезапуск SBCL и запуск текущего кода"));
    }

    // ── Tools Menu ─────────────────────────────────────
    if (m_lightStyleAction) m_lightStyleAction->setText(tr("Светлая"));
    if (m_darkStyleAction) m_darkStyleAction->setText(tr("Тёмная"));

    // ── ToolBar ────────────────────────────────────────
    if (m_mainToolBar) m_mainToolBar->setWindowTitle(tr("Панель инструментов"));

    // ── Actions по objectName ─────────
    auto setActionText = [this](QString name, const QString& text) {

        if (auto* action = findChild<QAction*>(name, Qt::FindChildOption(Qt::FindDirectChildrenOnly | Qt::FindChildrenRecursively))) {
            action->setText(text);
        }
        };

    setActionText("actionExit", tr("Выйти"));
    setActionText("actionFullScreen", tr("Полноэкранный режим"));
    setActionText("actionSettings", tr("Настройки"));
    setActionText("actionAbout", tr("О программе"));
}