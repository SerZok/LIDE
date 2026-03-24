#include "console_parser.h"
#include <QRegularExpression>

// фильтр вывода
QString filterOutput(const QString& raw)
{
    QStringList lines = raw.split('\n');
    QStringList result;

    for (QString line : lines)
    {
        QString t = line.trimmed();

        if (t.isEmpty()) continue;

        // ===== 1. УБИРАЕМ ЯВНЫЙ МУСОР =====
        if (t.startsWith("*")) continue;
        if (t.startsWith("CL-USER>")) continue;
        if (t.startsWith("Unhandled")) continue;
        if (t.startsWith("Backtrace")) continue;
        if (t.contains("SB-THREAD")) continue;
        if (t.contains("SB-IMPL::")) continue;
        if (t.contains("SB-INT::")) continue;
        if (t.contains("REPL-FUN")) continue;
        if (t.contains("foreign function")) continue;
        if (t.contains("unhandled condition")) continue;

        // ===== 2. УБИРАЕМ BACKTRACE =====
        if (QRegularExpression("^\\d+:").match(t).hasMatch())
            continue;

        // ===== 3. УБИРАЕМ ТЕХНИЧЕСКИЕ СТРОКИ =====
        if (t.startsWith("Stream:")) continue;
        if (t.contains("{") && t.contains("}>")) continue;

        // ===== 4. ЧИСТИМ МЕЛКИЙ МУСОР =====
        line.remove(QRegularExpression("\\b\\d+\\]"));
        line = line.trimmed();

        // ===== 5. ОСТАВЛЯЕМ =====
        result.append(line);
    }

    return result.join("\n").trimmed();
}

SBCLMessage ConsoleParser::parse(const QString& sbclOutput) {
    SBCLMessage msg;

    if (sbclOutput.isEmpty()) {
        msg.type = SBCLMessageType::Unknown;
        msg.message = "Empty output";
        return msg;
    }

    QString cleanedOutput = filterOutput(sbclOutput);

    // 1. Reader error (синтаксическая ошибка)
    QRegularExpression readerPattern("(unmatched close parenthesis)|(unexpected EOF.*)|(unexpected token.*)");
    QRegularExpressionMatch readerMatch = readerPattern.match(sbclOutput);
    if (readerMatch.hasMatch()) {
        msg.type = SBCLMessageType::ReaderError;
        msg.message = "Syntax Error: " + readerMatch.captured(0);

        // попытка найти caret '^'
        QRegularExpression caret_re("\\n\\s*\\^");
        QRegularExpressionMatch caretMatch = caret_re.match(sbclOutput);
        if (caretMatch.hasMatch()) {
            msg.column = caretMatch.capturedStart();
        }

        // финальный формат для студента
        QString formatted = msg.message + "\n" + cleanedOutput;
        if (msg.column.has_value()) {
            formatted += "\n" + QString(" ").repeated(msg.column.value()) + "^";
        }
        msg.message = formatted;
        return msg;
    }

    // 2. Runtime error (разного вида)
    QRegularExpression runtimePattern("(invalid number of arguments:\\s*\\d+)|(division by zero)|(type error.*)");
    QRegularExpressionMatch runtimeMatch = runtimePattern.match(sbclOutput);
    if (runtimeMatch.hasMatch()) {
        msg.type = SBCLMessageType::RuntimeError;
        msg.message = "Runtime Error: " + runtimeMatch.captured(0);
        msg.message += "\n" + cleanedOutput; // исходный код для контекста
        return msg;
    }

    QRegularExpression undefinedFuncPattern("undefined function: .*");
    QRegularExpressionMatch undefinedFuncMatch = undefinedFuncPattern.match(sbclOutput);
    if (undefinedFuncMatch.hasMatch()) {
        msg.type = SBCLMessageType::RuntimeError;
        msg.message = "Runtime Error: undefined function " + undefinedFuncMatch.captured(0).split(": ").last();
        return msg;
    }

    // 3. Любая другая ошибка (не распознана, но содержит 'error')
    if (sbclOutput.contains("error", Qt::CaseInsensitive)) {
        msg.type = SBCLMessageType::RuntimeError;
        msg.message = "Runtime Error occurred\n" + cleanedOutput;
        return msg;
    }

    // 4. Успех — результат программы
    msg.type = SBCLMessageType::Success;
    msg.message = cleanedOutput;
    return msg;
}