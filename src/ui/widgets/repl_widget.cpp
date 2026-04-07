#include "repl_widget.h"

#include <QKeyEvent>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QTextBlock>

ReplWidget::ReplWidget(QWidget* parent)
    : QTextEdit(parent)
    , m_controller(new ReplController(this))
    , m_settings(Settings::instance())
{
    setLineWrapMode(QTextEdit::NoWrap);
    setObjectName("ReplWidget");

    MAX_LINES = m_settings->replMaxLines();

    setupConnections();
    m_controller->start();
    appendPrompt();
}

ReplWidget::~ReplWidget()
{
    m_controller->stop();
}

void ReplWidget::setupConnections()
{
    connect(this, &ReplWidget::commandEntered, m_controller, &ReplController::sendCommand);
    connect(m_controller, &ReplController::messageReady, this, &ReplWidget::displayMessage);
    connect(m_controller, &ReplController::started, this, &ReplWidget::onControllerStarted);
    connect(m_controller, &ReplController::finished, this, &ReplWidget::onControllerFinished);
    connect(m_controller, &ReplController::errorLocationAvailable, this, &ReplWidget::onErrorLocationAvailable);
}

bool ReplWidget::hasPromptAtEditableStart() const
{
    QTextCursor cursor = textCursor();
    cursor.setPosition(m_editableStart);
    cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);

    QString textBeforeEditable = cursor.selectedText();
    return textBeforeEditable.trimmed() == m_prompt.trimmed() ||
        textBeforeEditable.endsWith(m_prompt);
}

void ReplWidget::displayMessage(const ReplMessage& msg)
{
    bool hasPrompt = hasPromptAtEditableStart();
    QString inputtedText = "";
    if (hasPrompt)
    {
        inputtedText = currentInput();
        clearCurrentLineAndPrompt();
    }
    switch (msg.type) {
    case ReplMessageType::Prompt:
        m_waitingForInput = true;
        hasPrompt = true;
        setReadOnly(false);
        break;
    case ReplMessageType::Result:
        appendOutput(msg.text + "\n", msg.type);
        break;
    case ReplMessageType::Error:
        appendOutput(msg.text + "\n", msg.type);
        if(m_controller && m_controller->isRunning())
            m_waitingForInput = true;
        else
            m_waitingForInput = false;
        break;
    case ReplMessageType::Warning:
        appendOutput(msg.text + "\n", msg.type);
        break;
    case ReplMessageType::Output:
        appendOutput(msg.text, msg.type);
        break;
    default:
        break;
    }
    if (hasPrompt)
    {
        appendPrompt();
        if (m_waitingForInput)
        {
            appendOutput(inputtedText);
            setReadOnly(false);
        }
    }
}

void ReplWidget::sendCode(const QString& code)
{
    if (!code.isEmpty()) {
        emit commandEntered(code);
    }
}

void ReplWidget::sendFile(const QString& path)
{
    clear();
    if (path.isEmpty()) {
        appendOutput("ОШИБКА: путь к файлу не указан\n", ReplMessageType::Error);
        return;
    }
    QString loadArgs = Settings::instance()->sbclLoadArgsString();
    QString command = QString("(load \"%1\"%2)").arg(path, loadArgs);
    m_last_usage_file = path;
    emit commandEntered(command);
}

void ReplWidget::restart()
{
    m_controller->restart();
}

void ReplWidget::stop()
{
    m_controller->stop();
}

void ReplWidget::clear()
{
    QTextEdit::clear();
    m_editableStart = 0;

    if (!m_prompt.isEmpty()) {
        m_waitingForInput = true;
    }
}

void ReplWidget::appendOutput(const QString& text, ReplMessageType type)
{
    if (toPlainText().length() > MAX_LINES) {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 1000);
        cursor.removeSelectedText();
    }

    QTextCursor cursor(document());
    cursor.movePosition(QTextCursor::End);
    setTextCursor(cursor);

    QTextCharFormat fmt;
    switch (type) {
        break;
    case ReplMessageType::Error:
        fmt.setForeground(Qt::red);
        break;
    case ReplMessageType::Warning:
        fmt.setForeground(QColor(255, 200, 0));
        break;
    case ReplMessageType::Prompt:
    case ReplMessageType::Output:
    case ReplMessageType::Result:
    default:
        fmt.setForeground(Qt::white);
        break;
    }
    textCursor().insertText(text, fmt);

    ensureCursorVisible();
}

void ReplWidget::appendPrompt()
{
    if (!m_waitingForInput) return;

    moveCursor(QTextCursor::End);
    if (!toPlainText().endsWith("\n") && !document()->isEmpty())
        insertPlainText("\n");

    // Сохраняем текущий цвет
    QTextCursor cursor = textCursor();

    // Вставляем зеленый промпт
    cursor.insertText(m_prompt, [&]() {
        QTextCharFormat format;
        format.setForeground(QColor(0, 255, 0));
        return format;
        }());

    // Меняем формат для следующего ввода
    cursor.setCharFormat([&]() {
        QTextCharFormat format;
        format.setForeground(Qt::white);
        return format;
        }());

    document()->clearUndoRedoStacks(QTextDocument::UndoAndRedoStacks);

    m_editableStart = textCursor().position();

}

bool ReplWidget::isFormComplete(const QString& text)
{
    int paren = 0;
    bool inString = false;

    for (QChar c : text) {
        if (c == '"') {
            inString = !inString;
            continue;
        }

        if (inString)
            continue;

        if (c == '(') paren++;
        if (c == ')') paren--;
    }

    return paren <= 0;
}

QString ReplWidget::currentLine() const
{
    if (m_editableStart < 0 || m_editableStart > document()->characterCount()) {
        return QString();
    }

    QTextCursor cursor = textCursor();
    cursor.setPosition(m_editableStart);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);

    QString text = cursor.selectedText();
    text.replace(QChar(0x2029), '\n');
    return text.trimmed();
}

QString ReplWidget::currentInput() const
{
    QTextCursor cursor(document());
    cursor.setPosition(m_editableStart);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    QString text = cursor.selectedText();
    text.replace(QChar(0x2029), '\n');
    return text.trimmed();
}

void ReplWidget::sendCurrentLine()
{
    QString input = currentInput();

    if (!isFormComplete(input)) {
        insertPlainText("\n");
        return;
    }

    if (input.trimmed().isEmpty()) {
        appendPrompt();
        return;
    }

    if (!input.startsWith(";"))
        m_history.add(input);

    if (input == "clear") {
        clear();
        appendPrompt();
        return;
    }

    insertPlainText("\n");
    emit commandEntered(input);

    setReadOnly(true);
    m_waitingForInput = false;
    m_historyBrowsing = false;
    m_savedInput.clear();
}

void ReplWidget::insertFromHistory(int direction)
{
    if (!m_historyBrowsing) {
        m_savedInput = currentLine();
        m_historyBrowsing = true;
    }

    QString cmd = (direction < 0) ? m_history.previous() : m_history.next();

    ensureCursorInEditable();

    // Удаление текущего ввода
    QTextCursor cursor = textCursor();
    cursor.setPosition(m_editableStart);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();

    if (cmd.isEmpty()) {
        // дошли до конца истории → возвращаем сохранённый ввод
        cursor.insertText(m_savedInput);
        m_historyBrowsing = false;
    }
    else {
        cursor.insertText(cmd);
    }

    setTextCursor(cursor);
}

void ReplWidget::ensureCursorInEditable()
{
    QTextCursor cursor = textCursor();
    if (cursor.position() < m_editableStart) {
        cursor.setPosition(m_editableStart);
        setTextCursor(cursor);
    }
}

void ReplWidget::keyPressEvent(QKeyEvent* event)
{
    // Ctrl+C — прерывание или копирование
    if (event->key() == Qt::Key_C && event->modifiers() == Qt::ControlModifier) {
        if (textCursor().hasSelection()) {
            copy();
        }
        else {
            m_controller->interrupt();
            //appendOutput("\n[ПРЕРВАНО] Выполнение прервано пользователем.\n", true);
            //appendPrompt();
        }
        return;
    }

    // Ctrl+V — вставка
    if (event->key() == Qt::Key_V && event->modifiers() == Qt::ControlModifier) {
        ensureCursorInEditable();
        QTextEdit::keyPressEvent(event);
        return;
    }

    // Shift+Enter — новая строка
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == Qt::ShiftModifier) {
        ensureCursorInEditable();
        insertPlainText("\n");
        return;
    }

    // Enter — отправка
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        sendCurrentLine();
        return;
    }

    // Стрелки для истории
    if (event->key() == Qt::Key_Up) {
        insertFromHistory(-1);
        return;
    }
    if (event->key() == Qt::Key_Down) {
        insertFromHistory(1);
        return;
    }

    // ЗАЩИТА ОТ УДАЛЕНИЯ
    if (event->key() == Qt::Key_Backspace) {
        QTextCursor cursor = textCursor();

        // Если есть выделение — проверяем, что выделение полностью в editable области
        if (cursor.hasSelection()) {
            int start = cursor.selectionStart();
            int end = cursor.selectionEnd();

            // Выделение только в editable области — разрешаем
            if (start >= m_editableStart && end >= m_editableStart) {
                QTextEdit::keyPressEvent(event);
            }
            else if (start < m_editableStart && end > m_editableStart) {
                return;
            }
            else {
                return;
            }
        }
        else {
            // Без выделения — проверяем позицию курсора
            if (cursor.position() <= m_editableStart) {
                return;
            }
        }

        QTextEdit::keyPressEvent(event);
        return;
    }

    // ЗАЩИТА ОТ УДАЛЕНИЯ
    if (event->key() == Qt::Key_Delete) {
        QTextCursor cursor = textCursor();

        // Если есть выделение — проверяем, что выделение полностью в editable области
        if (cursor.hasSelection()) {
            int start = cursor.selectionStart();
            int end = cursor.selectionEnd();

            // Выделение только в editable области — разрешаем
            if (start >= m_editableStart && end >= m_editableStart) {
                QTextEdit::keyPressEvent(event);
            }
            else if (start < m_editableStart && end > m_editableStart) {
                return;
            }
            else {
                return;
            }
        }
        else {
            // Без выделения — проверяем позицию курсора
            if (cursor.position() < m_editableStart) {
                return;
            }
        }

        QTextEdit::keyPressEvent(event);
        return;
    }

    // Обычный ввод
    ensureCursorInEditable();
    if (event->key() == Qt::Key_Left)
    {
        QTextCursor cursor = textCursor();
        if(cursor.position() > m_editableStart)
            QTextEdit::keyPressEvent(event);
        return;
    }
    QTextEdit::keyPressEvent(event);
}

void ReplWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu * menu = new QMenu(this);

    QAction* copyAction = menu->addAction(tr("Копировать"));
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setEnabled(textCursor().hasSelection());
    connect(copyAction, &QAction::triggered, this, &ReplWidget::copy);

    QAction* pasteAction = menu->addAction(tr("Вставить"));
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, [this]() {
        ensureCursorInEditable();
        paste();
        });

    menu->addSeparator();

    QAction* clearAction = menu->addAction(tr("Очистить консоль"));
    connect(clearAction, &QAction::triggered, this, &ReplWidget::clear);

    QAction* restartAction = menu->addAction(tr("Перезапустить REPL"));
    connect(restartAction, &QAction::triggered, this, &ReplWidget::restart);

    menu->exec(event->globalPos());
    delete menu;
}

void ReplWidget::onControllerStarted()
{
    qDebug() << "=== SBCL запущен ===";
    m_waitingForInput = true;
}

void ReplWidget::onControllerFinished()
{
    qDebug() << "=== SBCL остановлен ===";
}

void ReplWidget::onErrorLocationAvailable(const QString& message, int line, int column)
{
    if (!m_last_usage_file.isEmpty()) {
        emit errorLocationAvailable(m_last_usage_file, message, line, column);
    }
}

void ReplWidget::clearCurrentLineAndPrompt()
{
    QTextCursor cursor = textCursor();

    // Перемещаемся в позицию m_editableStart (начало ввода, после промпта)
    cursor.setPosition(m_editableStart);

    // 1. Удаляем всё от m_editableStart до конца документа
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();

    // 2. Теперь удаляем промпт (от начала строки до m_editableStart)
    cursor.setPosition(m_editableStart);
    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();    

    // Обновляем позицию начала редактирования
    m_editableStart = textCursor().position();
}