#pragma once

#include <QObject>
#include "repl_message.h"

class ReplParser : public QObject
{
    Q_OBJECT

public:
    explicit ReplParser(QObject* parent = nullptr);

    void pushData(const QString& chunk);   // добавить порцию данных
    void reset();                          // сбросить состояние
    void setPrompt(const QString& prompt); // установить Prompt

signals:
    void messageReady(const ReplMessage& msg);

private:
    QString m_buffer;
    QString m_currentPrompt = "CL-USER> ";

    void processLine(const QString& line);
    ReplMessage parseError(const QString& line) const;
    bool isPrompt(const QString& line) const;
    bool isTechnicalLine(const QString& line) const;
    ReplMessage parseResult(const QString& line) const;
};