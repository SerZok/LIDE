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
    switch (msg.type) {
    case ReplMessageType::Prompt:
        m_prompt = msg.text;
        m_waitingForInput = true;
        appendPrompt();
        break;
    case ReplMessageType::Result:
        appendOutput(msg.text + "\n", msg.type);
        break;
    case ReplMessageType::Error:
        appendOutput(msg.text + "\n", msg.type);
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

    if (!m_prompt.isEmpty()) {
        m_waitingForInput = true;
        appendPrompt();
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

    moveCursor(QTextCursor::End);

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
    if (!document()->isEmpty() && !textCursor().atBlockStart()) {
        insertPlainText("\n");
    }

    QColor promptColor = QColor(0, 255, 0);
    setTextColor(promptColor);
    insertPlainText(m_prompt);

    setTextColor(palette().color(QPalette::Text));

    m_editableStart = textCursor().position();

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
    //if (line.isEmpty()) {
    //    insertPlainText("\n");
    //    emit commandEntered("");
    //    m_waitingForInput = false;
    //    return;
    //}
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

    QAction* enableDebug = menu->addAction(tr("Режим отладки"));
    enableDebug->setCheckable(true);
    enableDebug->setChecked(m_controller->debugMode());
    connect(enableDebug, &QAction::triggered, [this](bool checked) {
        m_controller->setDebugMode(checked);
        });

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