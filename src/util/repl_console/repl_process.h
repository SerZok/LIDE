#pragma once

#include <QObject>
#include <QString>
#include <QProcess>

class ReplProcess : public QObject
{
    Q_OBJECT

public:
    explicit ReplProcess(QObject* parent = nullptr);
    ~ReplProcess();

    bool start(const QString& sbclPath = "", bool debugMode = false);
    void stop();
    void restart();

    void send(const QString& data); // Отправляет данные в SBCL
    void interrupt();  // Ctrl+C
    bool isRunning() const;

signals:
    void stdoutData(const QString& data);
    void stderrData(const QString& data);
    void started();
    void finished();
    void errorOccurred(const QString& error);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

private:
    QProcess m_process;
    QString m_pendingOutput;  // буфер для незавершённых строк
    bool m_debugMode;

    QString findSBCL() const;
    void flushPending(); // Обрабатывает построчный вывод
};