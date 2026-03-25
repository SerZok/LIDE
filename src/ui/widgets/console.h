#pragma once

#include <QTextEdit>
#include <QProcess>
#include <QKeyEvent>
#include <QDebug>
#include <QTextBlock>
#include "components/console_parser.h"

class Console : public QTextEdit
{
    Q_OBJECT

public:
    explicit Console(QWidget* parent = nullptr);
    ~Console();

    // Запуск/остановка процесса
    bool startLispProcess();
    void stopLispProcess();
    bool isRunning() const { return m_process && m_process->state() == QProcess::Running; }

    // Отправка команд
    void sendCommand(const QString& command);
    void sendCurrentCommandLine();  // Отправить текущую строку
    void sendCode(const QString& code); // Отправить код 'code'
    void computeCodeFile(const QString& path); // Выполнить код из файла path (с полным путём, например "/home/user/mycode.lisp")
    void sendSelectedText(); // Отправить выделенный текст

    // Получить информацию о последней ошибке
    SBCLMessage getLastDetailedInfo() const { return m_lastInfo; }

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessStateChanged(QProcess::ProcessState state);

signals:
    void processStateChanged(QProcess::ProcessState state);

private:
    QProcess* m_process;
    QString m_currentCommand;
    QStringList m_commandHistory;
    int m_historyIndex;
    QString m_prompt;
    bool m_waitingForInput;
    bool m_formatted_output_on;
    bool m_debug_mode;
    QString m_last_output;

    // Позиция, от которой можно редактировать (после prompt)
    int m_editableStart;

    // Информация о последнем выполнении
    SBCLMessage m_lastInfo;

    void setupConsole();
    void appendOutput(const QString& text, bool isError = false, bool isNotice = false);
    void appendPrompt();
    QString getCurrentCommandLineText() const;
    void insertFromHistory(int direction); // -1 назад, +1 вперёд

    // Вспомогательные
    void ensureCursorInEditableArea();
    bool cursorBeforeEditable() const;
    void clear();
};