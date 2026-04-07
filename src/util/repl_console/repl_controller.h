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
	void setFormattedOutput(bool enabled);

signals:
	void messageReady(const ReplMessage& msg);
	void started();
	void finished();
	void errorLocationAvailable(const QString& message, int line, int column);

private slots:
	void onProcessStarted();
	void onProcessFinished();
	void onProcessError(const QString& error);
	void onParserMessage(const ReplMessage& msg);

private:
	ReplProcess process;
	ReplParser parser;
	Settings* m_settings;

	bool m_formattedOutput = true;
	bool m_restartPending = false;

	void setupConnections();
};