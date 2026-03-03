#pragma once

#include <QTextEdit>
#include <QProcess>
#include <QKeyEvent>
#include <QDebug>
#include <QTextBlock>

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
    void sendCurrentLine();  // Отправить текущую строку
    void sendSelectedText(); // Отправить выделенный текст

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
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

    void setupConsole();
    void appendOutput(const QString& text, bool isError = false);
    void appendPrompt();
    QString getCurrentLine() const;
    void clearCurrentLine();
    void insertFromHistory(int direction); // -1 назад, +1 вперёд
    void setupContextMenu();
};