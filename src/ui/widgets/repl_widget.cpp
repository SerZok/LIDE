#include "repl_widget.h"

#include <QKeyEvent>
#include <QMenu>
#include <QApplication>
#include <QClipboard>

ReplWidget::ReplWidget(QWidget* parent)
    : QTextEdit(parent)
    , m_controller(new ReplController(this))
{
    setLineWrapMode(QTextEdit::NoWrap);
    setObjectName("ReplWidget");

    setupConnections();
    m_controller->start();
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
}

void ReplWidget::displayMessage(const ReplMessage& msg)
{
    moveCursor(QTextCursor::End);

    switch (msg.type) {
    case ReplMessageType::Prompt:
        m_prompt = msg.text;
        m_waitingForInput = true;
        appendPrompt();
        break;
    case ReplMessageType::Result:
        appendOutput(msg.text + "\n", false);
        break;
    case ReplMessageType::Error:
        appendOutput(msg.text + "\n", true);
        break;
    case ReplMessageType::Output:
        appendOutput(msg.text, false);
        break;
    default:
        break;
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
        appendOutput("ОШИБКА: путь к файлу не указан\n", true);
        return;
    }

    QString command = QString("(load \"%1\" :verbose t :print t)").arg(path);
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
    m_waitingForInput = true;
    appendPrompt();
}

void ReplWidget::appendOutput(const QString& text, bool isError)
{
    moveCursor(QTextCursor::End);

    QTextCharFormat fmt;
    if (isError) {
        fmt.setForeground(Qt::red);
    }
    else {
        fmt.setForeground(Qt::white);
    }
    textCursor().insertText(text, fmt);
}

void ReplWidget::appendPrompt()
{
    if (!m_waitingForInput) return;

    moveCursor(QTextCursor::End);

    if (!document()->isEmpty() && !textCursor().atBlockStart()) {
        insertPlainText("\n");
    }

    setTextColor(Qt::green);
    insertPlainText(m_prompt);

    m_editableStart = textCursor().position();
    setTextColor(Qt::white);
}

QString ReplWidget::currentLine() const
{
    if (m_editableStart < 0 || m_editableStart > document()->characterCount()) {
        return QString();
    }

    QTextCursor cursor = textCursor();
    cursor.setPosition(m_editableStart);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    return cursor.selectedText().trimmed();
}

void ReplWidget::sendCurrentLine()
{
    QString line = currentLine();
    if (line.isEmpty()) {
        appendPrompt();
        return;
    }

    m_history.add(line);
    if (line == "clear") {
        clear();
        appendPrompt();
        return;
    }

    insertPlainText("\n");
    emit commandEntered(line);
    m_waitingForInput = false;
}

void ReplWidget::insertFromHistory(int direction)
{
    QString cmd = (direction < 0) ? m_history.previous() : m_history.next();
    if (cmd.isEmpty()) return;

    ensureCursorInEditable();
    QTextCursor cursor = textCursor();
    cursor.setPosition(m_editableStart);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(cmd);
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
    if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
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

    // Обычный ввод
    ensureCursorInEditable();
    QTextEdit::keyPressEvent(event);
}

void ReplWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu();

    menu->addSeparator();

    QAction* clearAction = menu->addAction(tr("Очистить консоль"));
    connect(clearAction, &QAction::triggered, this, &ReplWidget::clear);

    menu->addSeparator();

    QAction* restartAction = menu->addAction(tr("Перезапустить REPL"));
    connect(restartAction, &QAction::triggered, this, &ReplWidget::restart);

    menu->exec(event->globalPos());
    delete menu;
}

void ReplWidget::onControllerStarted()
{
    qDebug() << "=== SBCL запущен ===";
    m_waitingForInput = true;
    appendPrompt();
}

void ReplWidget::onControllerFinished()
{
    qDebug() << "=== SBCL остановлен ===";
}