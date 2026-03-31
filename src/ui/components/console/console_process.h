#pragma once
#include <QProcess>
#include <QObject>
#include "console_parser.h"

// Общение виджета с SBCL
class ConsoleProcess : public QObject
{
    Q_OBJECT

public:
    explicit ConsoleProcess();
    ~ConsoleProcess();

    // Запуск/остановка процесса
    bool startLispProcess();
    void stopLispProcess();
    bool isRunning() const { return m_process && m_process->state() == QProcess::Running; }

    // Отправка команд
    void sendCommand(const QString& command);

	bool debugMode() const { return m_debug_mode; }
	void setDebugMode(bool debug) { m_debug_mode = debug; }
	bool formattedOutputOn() const { return m_formatted_output_on; }
	void setFormattedOutputOn(bool on) { m_formatted_output_on = on; }

private:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessStateChanged(QProcess::ProcessState state);

signals:
    // Сигнал для передачи "сырого" текста в UI (Console::appendOutput ожидает такой формат)
    void rawOutput(const QString& text, bool isError = false, bool isNotice = false);

    // Сигнал с уже распарсенным сообщением SBCL
    void sbclMessage(const SBCLMessage& message);

    // Принято новое сообщение 
    void recievedNewOutputMessage(const QString& message);

    // вставить промпт пользователя
    void userPrompt(bool force = false);

    // вставить промпт sbcl
    void sbclPrompt();

    // Сигнал для указания ошибок с координатами (оставлен прежний формат)
    void errorOccurred(const QString& file, const QString& message, int line, int column);

    // Сигнал состояния процесса
    void processStateChanged(QProcess::ProcessState state);

private:
    bool m_debug_mode;
    QProcess* m_process;
    bool m_formatted_output_on;
};