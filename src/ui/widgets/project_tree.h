#pragma once

#include <QTreeView>
#include <QFileSystemModel>

class ProjectTree : public QTreeView
{
    Q_OBJECT

public:
    explicit ProjectTree(QWidget* parent = nullptr);
    ~ProjectTree();

    bool openProject(const QString& path);
    QString rootPath() const;
    void keyPressEvent(QKeyEvent* event);

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

    void setupFilter();
    void saveState();
    void restoreState();
};