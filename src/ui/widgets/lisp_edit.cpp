#include "lisp_editor.h"

#include <QPainter>
#include <QTextBlock>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

LispEditor::LispEditor(QWidget* parent)
    : QPlainTextEdit(parent)
    , m_lineNumberArea(new LineNumberArea(this))
{
    m_lineNumberArea->setObjectName("LineNumberArea");
    setTabStopDistance(40);
    setLineWrapMode(QPlainTextEdit::NoWrap);

    connect(this, &LispEditor::blockCountChanged, this, &LispEditor::updateLineNumberAreaWidth);
    connect(this, &LispEditor::updateRequest, this, &LispEditor::updateLineNumberArea);
    connect(this, &LispEditor::cursorPositionChanged, this, &LispEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

LispEditor::~LispEditor()
{
}

int LispEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 10 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void LispEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void LispEditor::updateLineNumberArea(const QRect& rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void LispEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void LispEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        // Цвет теперь из CSS, оставляем только прозрачность
        selection.format.setBackground(QColor(230, 230, 230, 20)); // 20 вместо 50
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void LispEditor::paintEvent(QPaintEvent* event)
{
    QPlainTextEdit::paintEvent(event);
}

// ==================[ LineNumberArea ]==================
void LispEditor::LineNumberArea::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), QColor(0, 0, 0));

    QTextBlock block = m_editor->firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(m_editor->blockBoundingGeometry(block).translated(m_editor->contentOffset()).top());
    int bottom = top + qRound(m_editor->blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            // Цвет текста теперь в CSS
            painter.setPen(QColor(255, 255, 255));
            painter.drawText(0, top, this->m_editor->m_lineNumberArea->width() - 5,
                fontMetrics().height(), Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(m_editor->blockBoundingRect(block).height());
        ++blockNumber;
    }
}

// ==================[ Работа с файлами ]==================
bool LispEditor::loadFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Не удалось открыть файл %1:\n%2").arg(fileName).arg(file.errorString()));
        return false;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    setPlainText(in.readAll());
    m_currentFile = fileName;
    document()->setModified(false);
    setWindowTitle(fileName);

    return true;
}

bool LispEditor::saveFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Не удалось сохранить файл %1:\n%2").arg(fileName).arg(file.errorString()));
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << toPlainText();
    document()->setModified(false);
    m_currentFile = fileName;
    setWindowTitle(fileName);

    return true;
}