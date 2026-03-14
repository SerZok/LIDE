#include "lisp_editor.h"

#include <QFile>
#include <QPainter>
#include <QTextBlock>
#include <QTextStream>
#include <QMessageBox>
#include <QFileInfo>

LispEditor::LispEditor(QWidget* parent)
    : QPlainTextEdit(parent)
    , m_lineNumberArea(new LineNumberArea(this))
    , m_fileWatcher(new QFileSystemWatcher(this))
    , m_reloadTimer(new QTimer(this))
    , m_ignoreChanges(false)
    , m_highlighter(new LispHighlighter(this->document()))
{
    m_lineNumberArea->setObjectName("LineNumberArea");
    setTabStopDistance(40);
    setLineWrapMode(QPlainTextEdit::NoWrap);

    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(200);

    connect(this, &LispEditor::blockCountChanged, this, &LispEditor::updateLineNumberAreaWidth);
    connect(this, &LispEditor::updateRequest, this, &LispEditor::updateLineNumberArea);
    connect(this, &LispEditor::cursorPositionChanged, this, &LispEditor::highlightCurrentLine);

    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &LispEditor::onFileChanged);
    connect(m_reloadTimer, &QTimer::timeout, this, &LispEditor::askForReload);
    connect(this, &LispEditor::textChanged, this, &LispEditor::onTextChanged);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

LispEditor::~LispEditor()
{
    if (!m_fileWatcher->files().isEmpty()) {
        m_fileWatcher->removePaths(m_fileWatcher->files());
    }
}

void LispEditor::themeChanged(QString& themeName) {
    if (m_highlighter)
        m_highlighter->onThemeChanged(themeName);
}

int LispEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 15 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
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
        selection.format.setBackground(QColor(230, 230, 230, 20));
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
    painter.fillRect(event->rect(), QColor(0, 0, 0, 0));

    QTextBlock block = m_editor->firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(m_editor->blockBoundingGeometry(block).translated(m_editor->contentOffset()).top());
    int bottom = top + qRound(m_editor->blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.drawText(0, top, this->m_editor->m_lineNumberArea->width() - 10, fontMetrics().height(), Qt::AlignRight, number);
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
    m_ignoreChanges = true;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    setPlainText(in.readAll());

    m_ignoreChanges = false;

    // Закрываем предыдущий
    if (!m_currentFile.isEmpty()) {
        emit fileClosed(m_currentFile);
    }

    m_currentFile = fileName;
    document()->setModified(false);

    updateWatcher();

    emit fileLoaded(fileName);
    emit fileModifiedChanged(fileName, false);

    return true;
}

bool LispEditor::saveFile()
{
    if (m_currentFile.isEmpty()) {
        return false;
    }

    return saveFileAs(m_currentFile);
}

bool LispEditor::saveFileAs(const QString& fileName)
{
    if (!m_fileWatcher->files().isEmpty()) {
        m_fileWatcher->removePaths(m_fileWatcher->files());
    }

    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Не удалось сохранить файл %1").arg(fileName));
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << toPlainText();
    file.close();

    QString oldFile = m_currentFile;
    m_currentFile = fileName;
    document()->setModified(false);

    updateWatcher();

    emit fileSaved(fileName);
    emit fileModifiedChanged(fileName, false);

    if (oldFile != fileName && !oldFile.isEmpty()) {
        emit fileClosed(oldFile);
    }

    return true;
}

void LispEditor::onFileChanged(const QString& path)
{
    if (m_ignoreChanges || path != m_currentFile) return;
    m_reloadTimer->start();
}

void LispEditor::onTextChanged()
{
    if (m_ignoreChanges) return;
    bool modified = document()->isModified();
    emit fileModifiedChanged(m_currentFile, modified);
}

void LispEditor::askForReload()
{
    if (!QFile::exists(m_currentFile)) {
        emit fileClosed(m_currentFile);
        m_currentFile.clear();
        return;
    }

    reloadFile();
}

bool LispEditor::reloadFile()
{
    if (m_currentFile.isEmpty())
        return false;

    loadFile(m_currentFile);
}

void LispEditor::updateWatcher()
{
    if (!m_fileWatcher->files().isEmpty()) {
        m_fileWatcher->removePaths(m_fileWatcher->files());
    }

    if (!m_currentFile.isEmpty() && QFile::exists(m_currentFile)) {
        m_fileWatcher->addPath(m_currentFile);
    }
}