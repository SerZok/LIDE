#pragma once

#include <QObject>
#include <QTimer>

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

private slots:
    void processBuffer();

private:
    QTimer* m_timer;
    QString m_buffer;
    QString m_currentPrompt;
    const int maxLinesPerTick = 100;

    void processLine(const QString& line);
    bool isPrompt(const QString& line) const;
    bool isTechnicalLine(const QString& line) const;
    bool isStarValue(const QString& line) const;

    ReplMessage parseError(const QString& line) const;
    ReplMessage parseResult(const QString& line) const;
};