#pragma once

#include "components/project_tree_delegate.h"

#include <QTreeView>
#include <QFileSystemModel>
#include "settings.h"

class ProjectTree : public QTreeView
{
    Q_OBJECT

public:
    explicit ProjectTree(QWidget* parent = nullptr);
    ~ProjectTree();

    bool openProject(const QString& path);
    QString rootPath() const;
    void keyPressEvent(QKeyEvent* event);

public slots:
    void onFileLoaded(const QString& path);
    void onFileSaved(const QString& path);
    void onFileModifiedChanged(const QString& path, bool modified);
    void onFileClosed(const QString& path);

signals:
    void fileActivated(const QString& path);
    void projectOpened(const QString& path);
    void openProjectRequested();

private slots:
    void onItemActivated(const QModelIndex& index);
    void onContextMenuRequested(const QPoint& pos);

private:
    QFileSystemModel* m_model;
    QAction* m_newFileAction;
    QAction* m_newFolderAction;
    QAction* m_openFileAction;
    QString m_rootPath;

    // Для отслеживания состояния файлов
    QSet<QString> m_modifiedFiles;
    QString m_currentFile;
    ProjectTreeDelegate* m_delegate;

    void saveState();
    void restoreState();
    void expandToPath(const QString& path);
    QModelIndex findFileIndex(const QString& path);
};