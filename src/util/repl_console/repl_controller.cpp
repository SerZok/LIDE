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
    if (process.isRunning()) {
        process.stop();
    }
}

void ReplController::restart()
{
    stop();
    start();
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

    process.send(command);
}

void ReplController::interrupt()
{
    process.interrupt();
}

void ReplController::setDebugMode(bool enabled)
{
    if (m_debugMode == enabled) return;
    m_debugMode = enabled;

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
    emit started();
}

void ReplController::onProcessFinished()
{
    emit finished();
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