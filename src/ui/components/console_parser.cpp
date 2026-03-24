#include "console_parser.h"
#include <QRegularExpression>

// ================= Фильтр вывода =================
QString filterOutput(const QString& raw)
{
    QStringList lines = raw.split('\n');
    QStringList result;

    for (QString line : lines)
    {
        QString t = line.trimmed();
        if (t.isEmpty()) continue;

        // ===== 1. УБИРАЕМ ЯВНЫЙ МУСОР =====
        if (t.startsWith("*")) continue;           // REPL маркеры
        if (t.startsWith("CL-USER>")) continue;   // REPL prompt
        if (t.startsWith("Unhandled")) continue;  // служебные ошибки
        if (t.startsWith("Backtrace")) continue;  // backtrace
        if (t.contains("foreign function")) continue;
        if (t.contains("unhandled condition")) continue;

        // ===== 2. УБИРАЕМ BACKTRACE =====
        if (QRegularExpression("^\\d+:").match(t).hasMatch())
            continue;

        // ===== 3. УБИРАЕМ ТЕХНИЧЕСКИЕ СТРОКИ =====
        if (t.startsWith("Stream:")) continue;
        if (t.contains("{") && t.contains("}>")) continue;

        // ===== 4. ЧИСТИМ МЕЛКИЙ МУСОР =====
        line.remove(QRegularExpression("\\b\\d+\\]")); // временные метки
        line = line.trimmed();

        // ===== 5. ОСТАВЛЯЕМ =====
        result.append(line);
    }

    return result.join("\n").trimmed();
}

// ================= Парсер SBCL =================
SBCLMessage ConsoleParser::parse(const QString& sbclOutput)
{
    SBCLMessage msg;

    if (sbclOutput.isEmpty()) {
        msg.type = SBCLMessageType::Unknown;
        msg.message = "Empty output";
        return msg;
    }

    QString cleanedOutput = filterOutput(sbclOutput);

    // ===== 1. Reader error =====
    QRegularExpression readerPattern(
        "(unmatched close parenthesis|unexpected EOF.*|unexpected token.*)",
        QRegularExpression::CaseInsensitiveOption);
    auto readerMatch = readerPattern.match(sbclOutput);
    if (readerMatch.hasMatch()) {
        msg.type = SBCLMessageType::ReaderError;
        msg.message = "Syntax Error: " + readerMatch.captured(0);

        // caret (если есть)
        QRegularExpression caret_re("\\n(\\s*)\\^");
        auto caretMatch = caret_re.match(sbclOutput);
        if (caretMatch.hasMatch()) {
            msg.column = caretMatch.captured(1).length();
        }

        return msg; // только полезное сообщение
    }

    // ===== 2. Runtime error =====
    QRegularExpression runtimePattern(
        "(invalid number of arguments:\\s*\\d+|division by zero|type error.*)",
        QRegularExpression::CaseInsensitiveOption);
    auto runtimeMatch = runtimePattern.match(sbclOutput);
    if (runtimeMatch.hasMatch()) {
        msg.type = SBCLMessageType::RuntimeError;
        msg.message = runtimeMatch.captured(0); // только суть ошибки
        return msg;
    }

    // ===== 3. Undefined function =====
    QRegularExpression undefPattern(
        "undefined function:\\s*(.*)",
        QRegularExpression::CaseInsensitiveOption);
    auto undefMatch = undefPattern.match(sbclOutput);
    if (undefMatch.hasMatch()) {
        msg.type = SBCLMessageType::RuntimeError;
        msg.message = "undefined function: " + undefMatch.captured(1);
        return msg;
    }

    // ===== 4. Unbound variable =====
    QRegularExpression unboundPattern(
        "The variable\\s+(.*?)\\s+is unbound\\.",
        QRegularExpression::CaseInsensitiveOption);
    auto unboundMatch = unboundPattern.match(sbclOutput);
    if (unboundMatch.hasMatch()) {
        msg.type = SBCLMessageType::RuntimeError;
        msg.message = "unbound variable: " + unboundMatch.captured(1);
        return msg;
    }

    // ===== 5. Любая другая ошибка =====
    if (sbclOutput.contains("error", Qt::CaseInsensitive)) {
        msg.type = SBCLMessageType::RuntimeError;
        msg.message = cleanedOutput;
        return msg;
    }

    // ===== 6. Success — результат программы =====
    msg.type = SBCLMessageType::Success;
    msg.message = cleanedOutput;
    return msg;
}