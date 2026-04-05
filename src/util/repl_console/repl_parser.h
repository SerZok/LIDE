#pragma once

#include <QObject>
#include <QTimer>

#include "settings.h"
#include "repl_message.h"

class ReplParser : public QObject
{
    Q_OBJECT

public:
    explicit ReplParser(QObject* parent = nullptr);
    void pushData(const QString& chunk);   // добавить порцию данных
    void reset();                          // сбросить состояние
    void setPrompt(const QString& prompt); // установить Prompt

    void setParserMode(int mode);

signals:
    void messageReady(const ReplMessage& msg);

private slots:
    void processBuffer();

private:
    Settings* m_settings;
    size_t m_maxLinesPerTick;
    Settings::ParseMode parseMode;

    QTimer* m_timer;
    QString m_buffer;
    QString m_currentPrompt;

    void processLine(const QString& line);
    bool isPrompt(const QString& line) const;
    bool isTechnicalLine(const QString& line) const;
    bool isStarValue(const QString& line) const;
    bool shouldFilterCommentLine(const QString& line) const;

    ReplMessage parseError(const QString& line) const;
    ReplMessage parseResult(const QString& line) const;
};