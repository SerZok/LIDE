#pragma once

#include "settings.h"

#include <QObject>
#include <QString>
#include <QProcess>

class ReplProcess : public QObject
{
    Q_OBJECT

public:
    explicit ReplProcess(QObject* parent = nullptr);
    ~ReplProcess();

    bool start();
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
    QStringList args;
    QString sbclPath;

    Settings* m_settings;
};