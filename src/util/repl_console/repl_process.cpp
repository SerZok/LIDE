#include "repl_process.h"

#include <QStandardPaths>
#include <QCoreApplication>
#include <QDir>
#include <QTimer>
#include <QDebug>

ReplProcess::ReplProcess(QObject* parent)
    : QObject(parent)
    , m_settings(Settings::instance())
{
    sbclPath = m_settings->sbclPath();
    args = m_settings->sbclArgs();

    connect(m_settings, &Settings::sbclPathChanged, this, [this]() {
        sbclPath = m_settings->sbclPath();
        restart();
        });
    
    connect(m_settings, &Settings::sbclArgsChanged, this, [this]() {
        args = m_settings->sbclArgs();
        restart();
        });

    connect(&m_process, &QProcess::readyReadStandardOutput, this, &ReplProcess::onReadyReadOutput);
    connect(&m_process, &QProcess::errorOccurred, this, &ReplProcess::onProcessError);
    connect(&m_process, &QProcess::finished, this, &ReplProcess::onProcessFinished);
}

ReplProcess::~ReplProcess()
{
    stop();
}

bool ReplProcess::start()
{
    if (isRunning()) {
        stop();
    }

    qDebug() << "Запуск: " << sbclPath << " Аргументы: " << args;

    m_process.setProcessChannelMode(QProcess::MergedChannels);
    m_process.setWorkingDirectory(QFileInfo(sbclPath).absolutePath());
    m_process.start(sbclPath, args);

    if (!m_process.waitForStarted(3000)) {
        emit errorOccurred("Failed to start SBCL (timeout)");
        return false;
    }

    emit started();
    return true;
}

void ReplProcess::stop()
{
    if (!isRunning()) return;
    m_stopping = true;
    qDebug() << "Stopping SBCL process...";
    m_process.terminate();

    if (!m_process.waitForFinished(1000)) {
        qDebug() << "Process didn't terminate, killing...";
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}   

void ReplProcess::restart()
{
    stop();
    start();
}

void ReplProcess::send(const QString& data)
{
    if (!isRunning()) {
        start();
        return;
    }

    QByteArray bytes = data.toUtf8();
    m_process.write(bytes);
}

void ReplProcess::interrupt()
{
    if (!isRunning()) return;

#ifdef Q_OS_WIN
    // Windows: принудительно завершаем
    m_process.kill();

    // Перезапускаем процесс
    QTimer::singleShot(100, this, [this]() {
        restart();
        });

    emit errorOccurred(tr("[Принудительное завершение]"));
#else
    // Unix: отправляем SIGINT
    ::kill(m_process.processId(), SIGINT);
    qDebug() << "SIGINT sent to process" << m_process.processId();
#endif
}

bool ReplProcess::isRunning() const
{
    return m_process.state() == QProcess::Running;
}

void ReplProcess::onReadyReadOutput()
{
    QByteArray data = m_process.readAllStandardOutput();
    if (!data.isEmpty()) {
        //qDebug() << "Сырые данные: " << data;
        emit stdoutData(QString::fromUtf8(data));
    }
}

void ReplProcess::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(status);

    qDebug() << "SBCL process finished";
    emit finished();

    if (m_stopping) {
        m_stopping = false;
        return;
    }
}

void ReplProcess::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = "[Start error]";
        break;
    case QProcess::Crashed:
        errorMsg = "[Crashed]";
        break;
    case QProcess::Timedout:
        errorMsg = "[Operation timed out]";
        break;
    case QProcess::WriteError:
        errorMsg = "[Write error]";
        break;
    case QProcess::ReadError:
        errorMsg = "[Read error]";
        break;
    default:
        errorMsg = "[Unknown error]";
    }

    emit errorOccurred(errorMsg);
}