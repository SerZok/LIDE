#include "console_parser.h"
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>

ConsoleParser::DetailedErrorInfo ConsoleParser::parseError(const QString& output)
{
    return parse(output);
}

ConsoleParser::DetailedErrorInfo ConsoleParser::parseCompileWarning(const QString& output)
{
    DetailedErrorInfo info;

    if (!output.contains("caught WARNING") &&
        !output.contains("caught ERROR") &&
        !output.contains("undefined variable")) {
        return info;
    }

    info.hasError = true;

    // 1. Извлекаем имя функции (in: DEFUN FUNCTION1)
    QRegularExpression funcRe("in:\\s+DEFUN\\s+(\\S+)");
    auto match = funcRe.match(output);
    if (match.hasMatch()) {
        info.function = match.captured(1);
    }

    // 2. Извлекаем выражение с ошибкой
    QRegularExpression exprRe(";\\s+(.+?)(?:\n|$)");
    match = exprRe.match(output);
    if (match.hasMatch()) {
        info.form = match.captured(1).trimmed();
    }

    // 3. Извлекаем тип ошибки
    QRegularExpression typeRe("caught\\s+(WARNING|ERROR):");
    match = typeRe.match(output);
    if (match.hasMatch()) {
        info.errorType = match.captured(1);
    }

    // 4. Извлекаем неопределенную переменную
    QRegularExpression varRe("undefined variable:\\s+(\\S+)");
    match = varRe.match(output);
    if (match.hasMatch()) {
        info.variable = match.captured(1);
        info.message = QString("undefined variable: %1").arg(info.variable);
    }

    // 5. Извлекаем количество предупреждений
    QRegularExpression countRe("caught\\s+(\\d+)\\s+WARNING");
    match = countRe.match(output);
    if (match.hasMatch()) {
        info.warningCount = match.captured(1).toInt();
    }

    return info;
}

ConsoleParser::DetailedErrorInfo ConsoleParser::parseRuntimeError(const QString& output)
{
    DetailedErrorInfo info;

    if (!output.contains("debugger invoked on")) {
        return info;
    }

    info.hasError = true;

    // Извлекаем тип ошибки
    QRegularExpression typeRe("debugger invoked on a ([^:]+):");
    auto match = typeRe.match(output);
    if (match.hasMatch()) {
        info.errorType = match.captured(1).trimmed();
    }

    // Извлекаем сообщение
    QRegularExpression msgRe(":\\s*(.+?)(?:\n|$)");
    match = msgRe.match(output);
    if (match.hasMatch()) {
        info.message = match.captured(1).trimmed();
    }

    // Извлекаем имя файла (если есть)
    QRegularExpression fileRe("file[:\"\\s]+([^\n\"]+\\.lisp)");
    match = fileRe.match(output);
    if (match.hasMatch()) {
        info.file = match.captured(1).trimmed();
    }

    // Извлекаем позицию (если есть)
    QRegularExpression posRe("position[\\s:]+(\\d+)");
    match = posRe.match(output);
    if (match.hasMatch()) {
        int pos = match.captured(1).toInt();
        info.column = pos;
        if (!info.file.isEmpty()) {
            info.line = positionToLine(info.file, pos);
        }
    }

    // Извлекаем рестарты
    info.restarts = extractRestarts(output).split("\n", Qt::SkipEmptyParts);

    return info;
}

ConsoleParser::DetailedErrorInfo ConsoleParser::parseUnmatchedParenthesis(const QString& output)
{
    DetailedErrorInfo info;

    if (!output.contains("unmatched close parenthesis")) {
        return info;
    }

    info.hasError = true;
    info.errorType = "SIMPLE-READER-ERROR";
    info.message = "unmatched close parenthesis";

    // Извлекаем информацию о потоке (может помочь)
    QRegularExpression threadRe("thread\\s+#<THREAD\\s+tid=(\\d+)");
    auto match = threadRe.match(output);
    if (match.hasMatch()) {
        // tid можно сохранить, но для координат это не даст
    }

    return info;
}

ConsoleParser::DetailedErrorInfo ConsoleParser::parseDebuggerInvoke(const QString& output)
{
    return parseRuntimeError(output);
}

ConsoleParser::DetailedErrorInfo ConsoleParser::parse(const QString& output)
{
    // Пытаемся распознать тип ошибки по порядку

    // 1. Ошибки компиляции/предупреждения
    if (output.contains("caught WARNING") || output.contains("caught ERROR")) {
        return parseCompileWarning(output);
    }

    // 2. Непарная скобка
    if (output.contains("unmatched close parenthesis")) {
        return parseUnmatchedParenthesis(output);
    }

    // 3. Отладчик (runtime ошибки)
    if (output.contains("debugger invoked on")) {
        return parseRuntimeError(output);
    }

    // 4. Другие ошибки
    if (output.contains("error", Qt::CaseInsensitive)) {
        DetailedErrorInfo info;
        info.hasError = true;
        info.message = output.trimmed();
        return info;
    }

    return DetailedErrorInfo();
}

QString ConsoleParser::extractRestarts(const QString& output)
{
    QStringList restarts;
    QRegularExpression restartRe("\\s+(\\d+):\\s+\\[([^\\]]+)\\](?:\\s+(.+))?");
    auto matches = restartRe.globalMatch(output);

    while (matches.hasNext()) {
        QRegularExpressionMatch match = matches.next();
        QString restart = QString("%1: [%2]").arg(match.captured(1), match.captured(2));
        if (match.captured(3).length() > 0) {
            restart += " " + match.captured(3);
        }
        restarts.append(restart);
    }

    return restarts.join("\n");
}

int ConsoleParser::positionToLine(const QString& filename, int position)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return -1;
    }

    QTextStream stream(&file);
    int currentPos = 0;
    int line = 1;

    while (!stream.atEnd()) {
        QString lineStr = stream.readLine();
        // +1 для символа новой строки
        if (currentPos + lineStr.length() + 1 > position) {
            file.close();
            return line;
        }
        currentPos += lineStr.length() + 1;
        line++;
    }

    file.close();
    return -1;
}

QString ConsoleParser::extractBetween(const QString& text, const QString& start, const QString& end)
{
    int startPos = text.indexOf(start);
    if (startPos == -1) return QString();

    startPos += start.length();
    int endPos = text.indexOf(end, startPos);
    if (endPos == -1) return QString();

    return text.mid(startPos, endPos - startPos).trimmed();
}

QList<QString> ConsoleParser::extractAllMatches(const QString& text, const QRegularExpression& regex)
{
    QList<QString> results;
    auto matches = regex.globalMatch(text);
    while (matches.hasNext()) {
        results.append(matches.next().captured(1));
    }
    return results;
}