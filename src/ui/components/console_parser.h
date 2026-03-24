#pragma once

#include <QString>
#include <QList>

class ConsoleParser
{
public:
    struct DetailedErrorInfo {
        bool hasError = false;
        QString file;
        int line = -1;
        int column = -1;
        QString function;
        QString form;
        QString errorType;
        QString message;
        QList<QString> restarts;
        QList<QString> stack;
        int warningCount = 0;
        QString variable;  // для undefined variable
    };

    // Парсинг ошибок из stderr
    static DetailedErrorInfo parseError(const QString& output);

    // Парсинг предупреждений компиляции
    static DetailedErrorInfo parseCompileWarning(const QString& output);

    // Парсинг runtime ошибок
    static DetailedErrorInfo parseRuntimeError(const QString& output);

    // Общий парсер (выбирает нужный метод)
    static DetailedErrorInfo parse(const QString& output);

    // Вспомогательные функции
    static int positionToLine(const QString& filename, int position);
    static QString extractRestarts(const QString& output);

private:
    // Разбор разных форматов ошибок
    static DetailedErrorInfo parseCompileTimeError(const QString& output);
    static DetailedErrorInfo parseUndefinedVariable(const QString& output);
    static DetailedErrorInfo parseUnmatchedParenthesis(const QString& output);
    static DetailedErrorInfo parseDebuggerInvoke(const QString& output);

    // Утилиты
    static QString extractBetween(const QString& text, const QString& start, const QString& end);
    static QList<QString> extractAllMatches(const QString& text, const QRegularExpression& regex);
};