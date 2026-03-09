#include "project_tree_delegate.h"
#include <QPainter>
#include <QFileSystemModel>
#include <QApplication>

ProjectTreeDelegate::ProjectTreeDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void ProjectTreeDelegate::setModifiedFiles(const QSet<QString>& files)
{
    m_modifiedFiles = files;
}

void ProjectTreeDelegate::setCurrentFile(const QString& file)
{
    m_currentFile = file;
}

void ProjectTreeDelegate::paint(QPainter* painter,const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    QString filePath = index.data(QFileSystemModel::FilePathRole).toString();

    if (!m_currentFile.isEmpty() && filePath == m_currentFile) {
        opt.font.setBold(true);
        opt.palette.setColor(QPalette::Highlight, QColor(0, 172, 161, 100));
        opt.palette.setColor(QPalette::HighlightedText, Qt::white);
    }

    QStyledItemDelegate::paint(painter, opt, index);

    // Если файл изменён, рисуем маркер поверх
    if (m_modifiedFiles.contains(filePath)) {
        painter->save();

        int dotSize = 8;
        int margin = 10;
        QRect rect = opt.rect;
        QPoint dotPos(rect.right() - margin - dotSize,
            rect.center().y() - dotSize / 2);

        painter->setBrush(QColor(255, 170, 0));
        painter->setPen(Qt::NoPen);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawEllipse(dotPos.x(), dotPos.y(), dotSize, dotSize);

        painter->restore();
    }
}