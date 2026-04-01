#include "repl_process.h"

#include <QStandardPaths>
#include <QCoreApplication>
#include <QDir>
#include <QTimer>
#include <QDebug>

ReplProcess::ReplProcess(QObject* parent)
    : QObject(parent)
    , m_debugMode(false)
{
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &ReplProcess::onReadyReadStandardOutput);
    connect(&m_process, &QProcess::readyReadStandardError, this, &ReplProcess::onReadyReadStandardError);
    connect(&m_process, &QProcess::errorOccurred, this, &ReplProcess::onProcessError);
    connect(&m_process, &QProcess::finished, this, &ReplProcess::onProcessFinished);
}

ReplProcess::~ReplProcess()
{
    stop();
}

QString ReplProcess::findSBCL() const
{
    // 1. Ищем встроенный SBCL (рядом с приложением)
#ifdef Q_OS_WIN
    QString bundled = QCoreApplication::applicationDirPath() + "/sbcl/sbcl.exe";
#else
    QString bundled = QCoreApplication::applicationDirPath() + "/sbcl/sbcl";
#endif

    if (QFile::exists(bundled)) {
        qDebug() << "Found bundled SBCL:" << bundled;
        return bundled;
    }

    // 2. Ищем в PATH
    QString systemExe = QStandardPaths::findExecutable("sbcl");
    if (!systemExe.isEmpty()) {
        qDebug() << "Found system SBCL:" << systemExe;
        return systemExe;
    }

    qDebug() << "SBCL not found!";
    return QString();
}

bool ReplProcess::start(const QString& sbclPath, bool debugMode)
{
    if (isRunning()) {
        stop();
    }

    m_debugMode = debugMode;

    QString exePath = sbclPath.isEmpty() ? findSBCL() : sbclPath;
    if (exePath.isEmpty()) {
        emit errorOccurred("SBCL not found. Please install SBCL or place it in the 'sbcl' folder.");
        return false;
    }

    // Аргументы запуска
    QStringList args;
    args << "--noinform";  // Убираем информационные сообщения

    if (!m_debugMode) {
        args << "--disable-debugger";  // Отключаем отладчик для обычного режима
    }

    m_process.setProcessChannelMode(QProcess::MergedChannels);
    m_process.setWorkingDirectory(QFileInfo(exePath).absolutePath());
    m_process.start(exePath, args);

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

    qDebug() << "Stopping SBCL process...";

    // Отключаем сигналы, чтобы избежать лишних вызовов
    disconnect(&m_process, nullptr, this, nullptr);

    m_process.terminate();
    if (!m_process.waitForFinished(1000)) {
        qDebug() << "Process didn't terminate, killing...";
        m_process.kill();
        m_process.waitForFinished(1000);
    }

    emit finished();
}

void ReplProcess::restart()
{
    stop();
    start("", m_debugMode);
}

void ReplProcess::send(const QString& data)
{
    if (!isRunning()) {
        start("", m_debugMode);
        return;
    }

    QByteArray bytes = data.toUtf8();
    m_process.write(bytes);
    m_process.waitForBytesWritten(100);
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

    emit errorOccurred("Process interrupted on Windows (restarted)");
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

void ReplProcess::onReadyReadStandardOutput()
{
    QByteArray data = m_process.readAllStandardOutput();
    if (data.isEmpty()) return;

    m_pendingOutput += QString::fromUtf8(data);
    flushPending();
}

void ReplProcess::onReadyReadStandardError()
{
    QByteArray data = m_process.readAllStandardError();
    if (data.isEmpty()) return;

    QString error = QString::fromUtf8(data);
    emit stderrData(error);
}

void ReplProcess::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(status);

    qDebug() << "SBCL process finished";

    // Если остались данные в буфере — отправляем их
    if (!m_pendingOutput.isEmpty()) {
        emit stdoutData(m_pendingOutput);
        m_pendingOutput.clear();
    }

    emit finished();
}

void ReplProcess::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = "Failed to start SBCL process";
        break;
    case QProcess::Crashed:
        errorMsg = "SBCL process crashed";
        break;
    case QProcess::Timedout:
        errorMsg = "Operation timed out";
        break;
    case QProcess::WriteError:
        errorMsg = "Write error";
        break;
    case QProcess::ReadError:
        errorMsg = "Read error";
        break;
    default:
        errorMsg = "Unknown error";
    }

    emit errorOccurred(errorMsg);
}

void ReplProcess::flushPending()
{
    if (!m_pendingOutput.isEmpty()) {
        emit stdoutData(m_pendingOutput);
        m_pendingOutput.clear();
    }
}