#pragma once

#include <QObject>

#include "util/repl_console/repl_message.h"
#include "util/repl_console/repl_parser.h"
#include "util/repl_console/repl_process.h"

class ReplController : public QObject
{
    Q_OBJECT

public:
    explicit ReplController(QObject* parent = nullptr);
    ~ReplController();

    void start();
    void stop();
    void restart();
    void sendCommand(const QString& cmd);
    void interrupt();

    bool debugMode();
    void setDebugMode(bool enabled);
    void setFormattedOutput(bool enabled);

signals:
    void messageReady(const ReplMessage& msg);
    void started();
    void finished();

private slots:
    void onProcessStarted();
    void onProcessFinished();
    void onProcessError(const QString& error);
    void onParserMessage(const ReplMessage& msg);

private:
    ReplProcess process;
    ReplParser parser;

    bool m_debugMode = false;
    bool m_formattedOutput = true;
    bool m_restartPending = false;

    void setupConnections();
};