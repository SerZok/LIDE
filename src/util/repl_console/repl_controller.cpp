#include "repl_controller.h"

#include <QDebug>

ReplController::ReplController(QObject* parent)
    : QObject(parent)
{
    setupConnections();
}

ReplController::~ReplController()
{
    stop();
}

void ReplController::setupConnections()
{
    connect(&process, &ReplProcess::stdoutData, &parser, &ReplParser::pushData);
    connect(&process, &ReplProcess::stderrData, &parser, &ReplParser::pushData);
    connect(&process, &ReplProcess::started, this, &ReplController::onProcessStarted);
    connect(&process, &ReplProcess::finished, this, &ReplController::onProcessFinished);
    connect(&process, &ReplProcess::errorOccurred, this, &ReplController::onProcessError);
    connect(&parser, &ReplParser::messageReady, this, &ReplController::onParserMessage);
}

void ReplController::start()
{
    if (!process.start("", m_debugMode)) {
        ReplMessage msg;
        msg.type = ReplMessageType::Error;
        msg.text = "Failed to start SBCL";
        emit messageReady(msg);
    }
}

void ReplController::stop()
{
    if (!process.isRunning())
        return;

    process.stop();
}

void ReplController::restart()
{
    if (process.isRunning()) {
        m_restartPending = true;
        stop();
    }
    else {
        start();
    }
}

void ReplController::sendCommand(const QString& cmd)
{
    if (cmd.isEmpty()) {
        interrupt();
        return;
    }

    QString command = cmd;
    if (!command.endsWith('\n')) {
        command += '\n';
    }

    // Нормализация переносов
    command.replace(QChar(0x2028), "\n");
    command.replace(QChar(0x2029), "\n");
    command.replace("\r\n", "\n");

    if (!command.endsWith('\n'))
        command += '\n';

    qDebug() << "Отправка команды:" << command;
    process.send(command);
}

void ReplController::interrupt()
{
    process.interrupt();
}

bool ReplController::debugMode() {
    return m_debugMode;
}

void ReplController::setDebugMode(bool enabled)
{
    if (m_debugMode == enabled) return;
    m_debugMode = enabled;

    parser.setDebugMode(enabled);

    if (process.isRunning()) {
        restart();
    }
}

void ReplController::setFormattedOutput(bool enabled)
{
    m_formattedOutput = enabled;
}

void ReplController::onProcessStarted()
{
    parser.reset();
    parser.setDebugMode(m_debugMode);
    emit started();
}

void ReplController::onProcessFinished()
{
    emit finished();

    if (m_restartPending) {
        m_restartPending = false;
        start();
    }
}

void ReplController::onProcessError(const QString& error)
{
    ReplMessage msg;
    msg.type = ReplMessageType::Error;
    msg.text = error;
    emit messageReady(msg);
}

void ReplController::onParserMessage(const ReplMessage& msg)
{
    emit messageReady(msg);
}