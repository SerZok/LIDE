#include "console.h"

#include <QScrollBar>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>
#include <QTextStream>

Console::Console(QWidget* parent)
    : QTextEdit(parent)
    , m_process(nullptr)
    , m_historyIndex(-1)
    , m_prompt("* ")
    , m_waitingForInput(true)
{
    setupConsole();
}

Console::~Console()
{
    stopLispProcess();
}

void Console::setupConsole()
{
    setReadOnly(false);
    setLineWrapMode(QTextEdit::NoWrap);

    // Начальный промпт
    appendPrompt();

    // Установка objectName для CSS
    setObjectName("Console");

    // История команд
    m_commandHistory.clear();
}

bool Console::startLispProcess()
{
    if (m_process && m_process->state() == QProcess::Running) {
        stopLispProcess();
    }

    m_process = new QProcess(this);

    connect(m_process, &QProcess::started, this, &Console::onProcessStarted);
    connect(m_process, &QProcess::finished, this, &Console::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &Console::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &Console::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &Console::onReadyReadStandardError);
    connect(m_process, &QProcess::stateChanged, this, &Console::onProcessStateChanged);

    // Запуск процесса
    QString exePath = QCoreApplication::applicationDirPath() + "/sbcl/sbcl.exe";
    m_process->start(exePath, QStringList() << "--noinform" << "--disable-debugger");

    return true;
}

void Console::stopLispProcess()
{
    if (m_process) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
        m_process->deleteLater();
        m_process = nullptr;
    }
}

void Console::sendCommand(const QString& command)
{
    if (!m_process || m_process->state() != QProcess::Running) {
        appendOutput("Lisp process not running. Starting...\n", true);
        startLispProcess();
        return;
    }

    // Добавляем команду в историю
    if (!command.isEmpty()) {
        m_commandHistory.append(command);
        m_historyIndex = m_commandHistory.size();
    }

    // Отображаем команду в консоли
    appendOutput(command + "\n");

    // Отправляем команду в процесс
    QString cmd = command;
    if (!cmd.endsWith('\n')) {
        cmd += '\n';
    }
    m_process->write(cmd.toUtf8());

    // Очищаем текущую строку
    clearCurrentLine();
    m_waitingForInput = false;
}

void Console::sendCurrentLine()
{
    QString line = getCurrentLine();
    if (!line.isEmpty()) {
        sendCommand(line);
    }
}

void Console::sendSelectedText()
{
    QString selected = textCursor().selectedText();
    if (!selected.isEmpty()) {
        sendCommand(selected);
    }
}

void Console::keyPressEvent(QKeyEvent* event)
{
    // Если не ждём ввод, игнорируем все клавиши, кроме специальных
    if (!m_waitingForInput) {
        switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_C:
            if (event->modifiers() == Qt::ControlModifier) {
                // Ctrl+C - прерывание
                if (m_process) {
                    m_process->kill();
                }
                return;
            }
            break;
        default:
            QTextEdit::keyPressEvent(event);
            return;
        }
    }

    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        sendCurrentLine();
        break;

    case Qt::Key_Up:
        insertFromHistory(-1);
        break;

    case Qt::Key_Down:
        insertFromHistory(1);
        break;

    case Qt::Key_Tab:
        // TODO: Автодополнение
        break;

    default:
        QTextEdit::keyPressEvent(event);
        break;
    }
}

void Console::mousePressEvent(QMouseEvent* event)
{
    // Запрещаем кликать в область истории
    QTextCursor cursor = cursorForPosition(event->pos());
    if (cursor.block().userState() != 1) { // 1 - маркер для editable области
        return;
    }
    QTextEdit::mousePressEvent(event);
}

void Console::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu();
    menu->addSeparator();

    QAction* clearAction = menu->addAction(tr("Clear Console"));
    connect(clearAction, &QAction::triggered, [this]() {
        clear();
        appendPrompt();
        });

    QAction* copyOutputAction = menu->addAction(tr("Copy Output"));
    connect(copyOutputAction, &QAction::triggered, [this]() {
        // TODO: Копирует только последний вывод
        });

    menu->exec(event->globalPos());
    delete menu;
}

void Console::onProcessStarted()
{
    appendOutput("Lisp process started.\n", false);
    m_waitingForInput = true;
    appendPrompt();
}

void Console::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString msg = QString("Lisp process finished with code %1\n").arg(exitCode);
    appendOutput(msg, true);
    m_waitingForInput = false;
}

void Console::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = "Failed to start Lisp process. Check if SBCL is installed.";
        break;
    case QProcess::Crashed:
        errorMsg = "Lisp process crashed.";
        break;
    case QProcess::Timedout:
        errorMsg = "Operation timed out.";
        break;
    case QProcess::WriteError:
        errorMsg = "Write error.";
        break;
    case QProcess::ReadError:
        errorMsg = "Read error.";
        break;
    default:
        errorMsg = "Unknown error.";
    }
    appendOutput("ERROR: " + errorMsg + "\n", true);
}

void Console::onReadyReadStandardOutput()
{
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromUtf8(data);
    appendOutput(output, false);
    m_waitingForInput = true;
    appendPrompt();
}

void Console::onReadyReadStandardError()
{
    QByteArray data = m_process->readAllStandardError();
    QString error = QString::fromUtf8(data);
    appendOutput(error, true);
}

void Console::onProcessStateChanged(QProcess::ProcessState state)
{
    QString stateStr;
    switch (state) {
    case QProcess::NotRunning:
        stateStr = "Not running";
        break;
    case QProcess::Starting:
        stateStr = "Starting...";
        break;
    case QProcess::Running:
        stateStr = "Running";
        break;
    }
    emit processStateChanged(state);
}

void Console::appendOutput(const QString& text, bool isError)
{
    QTextCursor cursor = textCursor();
    int position = cursor.position();

    moveCursor(QTextCursor::End);

    // Вставляем текст с соответствующим форматом
    if (isError) {
        setTextColor(Qt::red);
    }
    else {
        setTextColor(Qt::white);
    }
    insertPlainText(text);

    // Восстанавливаем курсор, если он был в редактируемой области
    if (m_waitingForInput) {
        moveCursor(QTextCursor::End);
    }
    else {
        cursor.setPosition(position);
        setTextCursor(cursor);
    }
}

void Console::appendPrompt()
{
    if (!m_waitingForInput) return;

    moveCursor(QTextCursor::End);
    setTextColor(Qt::green);
    insertPlainText(m_prompt);

    // Отмечаем эту строку как редактируемую
    textCursor().block().setUserState(1);
}

QString Console::getCurrentLine() const
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    QString line = cursor.selectedText();

    // Убираем промпт
    if (line.startsWith(m_prompt)) {
        line = line.mid(m_prompt.length());
    }
    return line.trimmed();
}

void Console::clearCurrentLine()
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
}

void Console::insertFromHistory(int direction)
{
    if (m_commandHistory.isEmpty()) return;

    m_historyIndex += direction;
    m_historyIndex = qBound(0, m_historyIndex, m_commandHistory.size() - 1);

    QString command = m_commandHistory[m_historyIndex];

    // Заменяем текущую строку
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(m_prompt + command);
}