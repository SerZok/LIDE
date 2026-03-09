#include "project_tree.h"
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QHeaderView>
#include <QInputDialog>
#include <QStandardPaths>
#include <QSettings>

// ==================[ ProjectTree ]==================
ProjectTree::ProjectTree(QWidget* parent)
    : QTreeView(parent)
    , m_model(new QFileSystemModel(this))
    , m_newFileAction(nullptr)
    , m_newFolderAction(nullptr)
    , m_openFileAction(nullptr)
{
    m_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);
    m_model->setNameFilters(QStringList() << "*.lisp" << "*.lsp" << "*.asd");
    m_model->setNameFilterDisables(false);

    setModel(m_model);

    setAnimated(true);
    setHeaderHidden(true);
    setExpandsOnDoubleClick(false);
    setContextMenuPolicy(Qt::CustomContextMenu);

    header()->hideSection(1); // Size
    header()->hideSection(2); // Type
    header()->hideSection(3); // Date

    connect(this, &QTreeView::doubleClicked, this, &ProjectTree::onItemActivated);
    connect(this, &QWidget::customContextMenuRequested, this, &ProjectTree::onContextMenuRequested);

    restoreState();
}

ProjectTree::~ProjectTree()
{
    saveState();
}

bool ProjectTree::openProject(const QString& path)
{
    QFileInfo info(path);
    if (!info.exists() || !info.isDir()) {
        return false;
    }

    m_rootPath = QDir::cleanPath(path);
    m_model->setRootPath(m_rootPath);

    QModelIndex rootIndex = m_model->index(m_rootPath);
    setRootIndex(rootIndex);
    setCurrentIndex(rootIndex);
    expandToDepth(1);
    scrollTo(rootIndex, QAbstractItemView::PositionAtTop);

    emit projectOpened(m_rootPath);
    return true;
}

QString ProjectTree::rootPath() const
{
    return m_rootPath;
}

void ProjectTree::onItemActivated(const QModelIndex& index)
{
    if (!index.isValid()) return;

    if (!m_model->isDir(index)) {
        QString filePath = m_model->filePath(index);
        emit fileActivated(filePath);
    }
    else {
        setExpanded(index, !isExpanded(index));
    }
}

void ProjectTree::onContextMenuRequested(const QPoint& pos)
{
    QModelIndex index = indexAt(pos);
    QMenu menu(this);

    if (index.isValid()) {
        QString path = m_model->filePath(index);
        bool isDir = m_model->isDir(index);

        if (isDir) {
            QAction* newFileAction = menu.addAction(tr("Новый файл..."));
            QAction* newFolderAction = menu.addAction(tr("Новая папка..."));

            connect(newFileAction, &QAction::triggered, [this, path]() {
                QString fileName = QInputDialog::getText(this, tr("Новый файл"), tr("Название файла:"));
                if (!fileName.isEmpty()) {
                    QFile file(path + "/" + fileName);
                    file.open(QIODevice::WriteOnly);
                    file.close();
                }
                });

            connect(newFolderAction, &QAction::triggered, [this, path]() {
                QString dirName = QInputDialog::getText(this, tr("Новая папка"), tr("Название папки:"));
                if (!dirName.isEmpty()) {
                    QDir().mkdir(path + "/" + dirName);
                }
                });

            menu.addSeparator();
        }
        else {
            QAction* openFileAction = menu.addAction(tr("Открыть"));
            connect(openFileAction, &QAction::triggered, [this, index]() {
                onItemActivated(index);
                });
        }

        QAction* renameAction = menu.addAction(tr("Переименовать"));
        QAction* deleteAction = menu.addAction(tr("Удалить"));
        menu.addSeparator();
        QAction* showInFolderAction = menu.addAction(tr("Показать в проводнике"));

        connect(renameAction, &QAction::triggered, [this, index]() {
            edit(index);
            });

        connect(deleteAction, &QAction::triggered, [this, index, path]() {
            if (QMessageBox::question(this, tr("Подтверждение"),
                tr("Удалить %1?").arg(path)) == QMessageBox::Yes) {
                m_model->remove(index);
            }
            });

        connect(showInFolderAction, &QAction::triggered, [path]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).path()));
            });
    }
    else {
        QAction* newFileAction = menu.addAction(tr("Новый файл..."));
        QAction* newFolderAction = menu.addAction(tr("Новая папка..."));

        connect(newFileAction, &QAction::triggered, [this]() {
            QString fileName = QInputDialog::getText(this, tr("Новый файл"), tr("Название файла:"));
            if (!fileName.isEmpty()) {
                QFile file(m_rootPath + "/" + fileName);
                file.open(QIODevice::WriteOnly);
                file.close();
            }
            });

        connect(newFolderAction, &QAction::triggered, [this]() {
            QString dirName = QInputDialog::getText(this, tr("Новая папка"), tr("Название папки:"));
            if (!dirName.isEmpty()) {
                QDir().mkdir(m_rootPath + "/" + dirName);
            }
            });
    }

    menu.exec(viewport()->mapToGlobal(pos));
}

void ProjectTree::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete) {
        QModelIndex index = currentIndex();
        if (index.isValid()) {
            QString path = m_model->filePath(index);
            if (QMessageBox::question(this, tr("Подтверждение"),
                tr("Удалить %1?").arg(path)) == QMessageBox::Yes) {
                m_model->remove(index);
            }
        }
    }

    QTreeView::keyPressEvent(event);
}

void ProjectTree::saveState()
{
    QSettings settings("LIDE", "LIDE");
    settings.setValue("project_tree/root_path", m_rootPath);
}

void ProjectTree::restoreState()
{
    QSettings settings("LIDE", "LIDE");
    QString lastPath = settings.value("project_tree/root_path").toString();

    if (!lastPath.isEmpty() && QDir(lastPath).exists()) {
        openProject(lastPath);
    }
    else {
        QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        openProject(documentsPath);
    }
}