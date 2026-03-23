#pragma once

#include <QStyledItemDelegate>
#include <QSet>

class ProjectTreeDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ProjectTreeDelegate(QObject* parent = nullptr);

    void setModifiedFiles(const QSet<QString>& files);
    void setCurrentFile(const QString& file);

protected:
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

private:
    QSet<QString> m_modifiedFiles;
    QString m_currentFile;
};