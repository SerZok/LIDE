#include "repl_widget.h"

#include <QMenu>
#include <QKeyEvent>
#include <QClipboard>
#include <QTextBlock>
#include <QApplication>
#include <QRegularExpression>

ReplWidget::ReplWidget(QWidget* parent)
    : QTextEdit(parent)
    , m_controller(new ReplController(this))
    , m_settings(Settings::instance())
{
    setLineWrapMode(QTextEdit::NoWrap);
    setObjectName("ReplWidget");

    MAX_LINES = m_settings->replMaxLines();

    loadThemeColors();

    setupConnections();
    m_controller->start();

    connect(m_settings, &Settings::themeChanged, this, [this]() {
        loadThemeColors();
        });
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
    moveCursor(QTextCursor::End);
    bool hasPrompt = hasPromptAtEditableStart();
    bool needPrompt = false;
    QString inputtedText = "";
    if (hasPrompt)
    {
        inputtedText = currentInput();
        clearCurrentLineAndPrompt();
    }
    switch (msg.type) {
    case ReplMessageType::Prompt:
        m_waitingForInput = true;
        needPrompt = true;
        hasPrompt = false;
        setReadOnly(false);
        break;
    case ReplMessageType::Result:
        appendOutput(msg.text + "\n", msg.type);
        hasPrompt = false;
        break;
    case ReplMessageType::Error:
        if (hasPrompt)
        {
            m_history.add(inputtedText);
            clearCurrentLineAndPrompt();
        }
        appendOutput(msg.text + "\n", msg.type);
        if (m_controller && m_controller->isRunning())
        {
            needPrompt = true;
            m_waitingForInput = true;
        }
        break;
    case ReplMessageType::Warning:
        if (hasPrompt)
            clearCurrentLineAndPrompt();
        if (m_controller && m_controller->isRunning())
        {
            needPrompt = true;
            m_waitingForInput = true;
        }
        appendOutput(msg.text + "\n", msg.type);
        break;
    case ReplMessageType::Output:
        appendOutput(msg.text, msg.type);
        break;
    default:
        break;
    }
    if (needPrompt)
    {
        appendPrompt();
        if (hasPrompt && m_waitingForInput)
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
    m_waitingForInput = true;
    appendPrompt();
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
    cursor.insertText(text, formatForType(type));
    setTextCursor(cursor);
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
    cursor.insertText(m_prompt, formatForType(ReplMessageType::Prompt));

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
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);

    QString text = cursor.selectedText();
    text.replace(QChar(0x2029), '\n');
    return text.trimmed();
}

void ReplWidget::sendCurrentInput()
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

    // Проверка: не добавляем дубликат последнего элемента
    if (m_history.isEmpty() || m_history.getAll().last() != input) {
        m_history.add(input);
    }

    m_history.resetToEnd();

    if (input == "clear") {
        clear();
        return;
    }

    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    setTextCursor(cursor);
    insertPlainText("\n");

    emit commandEntered(input);

    setReadOnly(true);
    appendPrompt();
    m_waitingForInput = false;
    m_historyBrowsing = false;
    m_savedInput.clear();
}

void ReplWidget::insertFromHistory(int direction)
{
    // Сохраняем текущий ввод только при первом входе в историю
    if (!m_historyBrowsing) {
        m_savedInput = currentInput();  // Используйте currentInput()? currentLine()?
        m_historyBrowsing = true;
    }

    QString cmd;
    if (direction < 0) { // Up
        cmd = m_history.previous();
    }
    else { // Down
        cmd = m_history.next();
    }

    ensureCursorInEditable();

    // Удаление текущего ввода
    QTextCursor cursor = textCursor();
    cursor.setPosition(m_editableStart);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();

    // Если команда пустая (дошли до конца при навигации вниз)
    if (cmd.isNull() || (direction > 0 && cmd.isEmpty())) {
        // Возвращаем сохранённый ввод
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

    // Enter — отправка
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() != Qt::ShiftModifier) {
        sendCurrentInput();
        ensureCursorVisible();
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

    if (event->key() == Qt::Key_Left)
    {
        QTextCursor cursor = textCursor();
        if (cursor.position() > m_editableStart)
            QTextEdit::keyPressEvent(event);
        return;
    }

    m_historyBrowsing = false; // Ставить перед клавишами редактирующими окно ввода

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
        ensureCursorVisible();
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
                return;
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

    // Проверяем, можно ли редактировать в текущей позиции
    if (!isEditAllowed()) {
        // Только навигация и копирование
        if (isNavigationKey(event)) {
            QTextEdit::keyPressEvent(event);
        }
        // Остальные клавиши игнорируем
        return;
    }

    // Здесь можно редактировать
    QTextEdit::keyPressEvent(event);
}

bool ReplWidget::isEditAllowed() const
{
    QTextCursor cursor = textCursor();

    // Если есть выделение — проверяем, что оно полностью внутри editable области
    if (cursor.hasSelection()) {
        return cursor.selectionStart() >= m_editableStart;
    }

    // Без выделения — проверяем позицию курсора
    return cursor.position() >= m_editableStart;
}

bool ReplWidget::isNavigationKey(QKeyEvent* event) const
{
    int key = event->key();
    return (key == Qt::Key_Left || key == Qt::Key_Right ||
        key == Qt::Key_Up || key == Qt::Key_Down ||
        key == Qt::Key_Home || key == Qt::Key_End ||
        key == Qt::Key_PageUp || key == Qt::Key_PageDown);
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

void ReplWidget::loadThemeColors()
{
    // Если стиль задан через qApp->setStyleSheet()
    QString styleSheet = qApp->styleSheet();

    // Если стиль пустой - используем значения по умолчанию
    if (styleSheet.isEmpty()) {
        // Определяем тему автоматически
        QColor bg = palette().color(QWidget::backgroundRole());
        bool isDark = bg.lightness() < 128;

        if (isDark) {
            m_promptColor = QColor(0, 255, 0);
            m_resultColor = QColor(224, 224, 224);
            m_errorColor = QColor(255, 107, 107);
            m_warningColor = QColor(255, 217, 61);
            m_outputColor = QColor(224, 224, 224);
        }
        else {
            m_promptColor = QColor(0, 136, 0);
            m_resultColor = QColor(45, 45, 45);
            m_errorColor = QColor(198, 38, 38);
            m_warningColor = QColor(166, 124, 0);
            m_outputColor = QColor(45, 45, 45);
        }
        return;
    }

    // Парсим цвета из qApp стилей
    auto extractColor = [&](const QString& propName, QColor& target, const QColor& fallback) {
        QRegularExpression re(QString(R"(qproperty-%1:\s*(#[0-9A-Fa-f]+))").arg(propName));
        auto match = re.match(styleSheet);
        target = match.hasMatch() ? QColor(match.captured(1)) : fallback;
        };

    extractColor("replPrompt", m_promptColor, QColor(0, 255, 0));
    extractColor("replResult", m_resultColor, QColor(224, 224, 224));
    extractColor("replError", m_errorColor, QColor(255, 107, 107));
    extractColor("replWarning", m_warningColor, QColor(255, 217, 61));
    extractColor("replOutput", m_outputColor, QColor(224, 224, 224));
}

QTextCharFormat ReplWidget::formatForType(ReplMessageType type) const
{
    QTextCharFormat fmt;
    switch (type) {
    case ReplMessageType::Prompt:   fmt.setForeground(m_promptColor); break;
    case ReplMessageType::Result:   fmt.setForeground(m_resultColor); break;
    case ReplMessageType::Error:    fmt.setForeground(m_errorColor); break;
    case ReplMessageType::Warning:  fmt.setForeground(m_warningColor); break;
    case ReplMessageType::Output:   fmt.setForeground(m_outputColor); break;
    default:                        fmt.setForeground(m_outputColor); break;
    }
    return fmt;
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