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

    m_matchedBracketFormat.setBackground(QColor(100, 150, 100, 100));
    m_matchedBracketFormat.setForeground(Qt::white);
    m_mismatchedBracketFormat.setBackground(QColor(200, 80, 80, 150));

    connect(this, &LispEditor::blockCountChanged, this, &LispEditor::updateLineNumberAreaWidth);
    connect(this, &LispEditor::updateRequest, this, &LispEditor::updateLineNumberArea);
    connect(this, &LispEditor::cursorPositionChanged, this, &LispEditor::highlightCurrentLine);;

    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &LispEditor::onFileChanged);
    connect(m_reloadTimer, &QTimer::timeout, this, &LispEditor::askForReload);
    connect(this, &LispEditor::textChanged, this, &LispEditor::onTextChanged);

    updateLineNumberAreaWidth(0);
}

LispEditor::~LispEditor()
{
    if (!m_fileWatcher->files().isEmpty()) {
        m_fileWatcher->removePaths(m_fileWatcher->files());
    }
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

QList<QTextEdit::ExtraSelection> LispEditor::highlightMatchingBrackets()
{
    // Получаем текущие подсветки (ошибки и текущую строку)
    QList<QTextEdit::ExtraSelection> extraSelections;

    QTextCursor cursor = textCursor();
    int position = cursor.position();

    // Пропускаем, если курсор в комментарии
    if (isPositionInComment(position)) {
        setExtraSelections(extraSelections);
        return extraSelections;
    }

    // ЕСЛИ ЕСТЬ ВЫДЕЛЕНИЕ - используем его начало
    if (cursor.hasSelection()) {
        position = cursor.selectionStart();
        cursor.setPosition(position);

        // Проверяем, не в комментарии ли начало выделения
        if (isPositionInComment(position)) {
            setExtraSelections(extraSelections);
            return extraSelections;
        }
    }

    if (position >= document()->characterCount() - 1) {
        setExtraSelections(extraSelections);
        return extraSelections;
    }

    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
    QString selectedText = cursor.selectedText();
    if (selectedText.isEmpty()) {
        setExtraSelections(extraSelections);
        return extraSelections;
    }

    QChar currentChar = selectedText.at(0);
    if (currentChar != '(' && currentChar != ')') {
        setExtraSelections(extraSelections);
        return extraSelections;
    }

    if (isPositionInComment(position)) {
        setExtraSelections(extraSelections);
        return extraSelections;
    }

    // Подсвечиваем текущую скобку
    QTextEdit::ExtraSelection currentSelection;
    currentSelection.cursor = cursor;
    currentSelection.format.setBackground(QColor(50, 50, 255, 200));
    extraSelections.append(currentSelection);

    // Ищем парную скобку
    int matchPos = findMatchingBracket(position, currentChar == '(');
    if (matchPos != -1) {
        if (!isPositionInComment(matchPos)) {
            // Подсвечиваем область между скобками
            QTextEdit::ExtraSelection regionSelection;
            QTextCursor regionCursor = textCursor();

            if (currentChar == '(') {
                regionCursor.setPosition(position + 1);
                regionCursor.setPosition(matchPos, QTextCursor::KeepAnchor);
            }
            else {
                regionCursor.setPosition(matchPos + 1);
                regionCursor.setPosition(position, QTextCursor::KeepAnchor);
            }

            if (!regionCursor.selectedText().isEmpty()) {
                regionSelection.cursor = regionCursor;
                regionSelection.format.setBackground(QColor(80, 80, 80, 50));
                extraSelections.append(regionSelection);
            }

            // Подсвечиваем парную скобку
            QTextEdit::ExtraSelection matchSelection;
            QTextCursor matchCursor(document());
            matchCursor.setPosition(matchPos);
            matchCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);

            matchSelection.cursor = matchCursor;
            matchSelection.format.setBackground(QColor(50, 50, 255, 200));
            extraSelections.append(matchSelection);
        }
    }
    else {
        // Если парная скобка не найдена, подсвечиваем ошибку
        QTextEdit::ExtraSelection errorSelection;
        errorSelection.cursor = cursor;
        errorSelection.format.setBackground(QColor(255, 50, 50));
        extraSelections.append(errorSelection);
    }

    return extraSelections;
}

int LispEditor::findMatchingBracket(int startPos, bool forward) const
{
    const QTextDocument* doc = document();
    int depth = 1;

    if (forward) {
        for (int i = startPos + 1; i < doc->characterCount(); ++i) {
            QChar c = doc->characterAt(i);

            if (c == '(') depth++;
            else if (c == ')') depth--;

            if (depth == 0) {
                return i;
            }
        }
    }
    else {
        for (int i = startPos - 1; i >= 0; --i) {
            QChar c = doc->characterAt(i);

            if (c == ')') depth++;
            else if (c == '(') depth--;

            if (depth == 0) {
                return i;
            }
        }
    }

    return -1;
}

bool LispEditor::isPositionInComment(int position) const
{
    const QTextDocument* doc = document();
    QTextBlock block = doc->findBlock(position);

    if (!block.isValid()) return false;

    QString text = block.text();
    int blockPosition = block.position();
    int relativePos = position - blockPosition;

    // Ищем первый символ ';' в строке
    int commentStart = text.indexOf(';');

    // Если комментарий начинается до текущей позиции
    return (commentStart != -1 && relativePos >= commentStart);
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
    // Получаем текущие подсветки (ошибки и т.д.)
    QList<QTextEdit::ExtraSelection> extraSelections = m_extraSelections;

    // Добавляем подсветку текущей строки
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor(230, 230, 230, 20));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    extraSelections.append(highlightMatchingBrackets());

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
    m_currentFile = fileName;
    document()->setModified(false);

    updateWatcher();

    emit fileLoaded(fileName);
    emit fileModifiedChanged(fileName, false);

    // После загрузки файла восстанавливаем подсветку
    updateErrorHighlight();

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

    return loadFile(m_currentFile);
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

void LispEditor::highlightErrorAtPosition(const QString& message,
    int line, int column)
{
    // Очищаем предыдущую подсветку
    clearErrorHighlight();

    // Находим блок по номеру строки
    QTextBlock block = document()->findBlockByNumber(line - 1);
    if (!block.isValid()) {
        qDebug() << "Invalid line:" << line;
        return;
    }

    // Создаём курсор для выделения позиции ошибки
    QTextCursor cursor(block);
    int errorPosition = block.position() + (column - 1);
    cursor.setPosition(errorPosition);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    // Подсветка позиции ошибки
    QTextEdit::ExtraSelection errorSelection;
    errorSelection.cursor = cursor;
    errorSelection.format.setBackground(QColor(255, 100, 100, 80));
    errorSelection.format.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    errorSelection.format.setUnderlineColor(QColor(255, 50, 50));
    errorSelection.format.setToolTip(QString("Line %1, Column %2: %3")
        .arg(line).arg(column).arg(message));

    // Подсветка всей строки
    QTextEdit::ExtraSelection lineHighlight;
    QTextCursor lineCursor(block);
    lineCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    lineHighlight.cursor = lineCursor;
    lineHighlight.format.setBackground(QColor(255, 100, 100, 40));
    lineHighlight.format.setProperty(QTextFormat::FullWidthSelection, true);

    // Сохраняем обе подсветки
    m_errorSelections.clear();
    m_errorSelections.append(errorSelection);
    m_errorSelections.append(lineHighlight);
    
    // Сохраняем основную подсветку для совместимости (если нужно)
    m_errorSelection = errorSelection;
    
    // Применяем подсветку
    updateErrorHighlight();

    // Прокручиваем к ошибке
    QTextCursor scrollCursor(document());
    scrollCursor.setPosition(errorPosition);
    setTextCursor(scrollCursor);
    ensureCursorVisible();
}

void LispEditor::clearErrorHighlight()
{
    m_errorSelection.cursor = QTextCursor();
    m_errorSelections.clear();
    updateErrorHighlight();
}

void LispEditor::updateErrorHighlight()
{
    // Просто копируем список подсветок
    m_extraSelections = m_errorSelections;
    setExtraSelections(m_extraSelections);
}