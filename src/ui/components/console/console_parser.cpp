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
        // ===== Обрабатываем строки со звездочкой =====
        if (t.startsWith("*")) {
            // Если после звездочки есть что-то кроме пробелов — это результат
            QString afterStar = t.mid(1).trimmed();
            if (!afterStar.isEmpty()) {
                // Это результат, сохраняем его
                result.append(afterStar);
            }
            continue;
        }
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
    msg.type = SBCLMessageType::Success;
    msg.message = sbclOutput;

    if (sbclOutput.isEmpty()) {
        msg.type = SBCLMessageType::Unknown;
        msg.message = "Empty output";
        return msg;
    }

    // ===== 1. Ищем позицию ошибки (строку, колонку, позицию в файле) =====
    // Формат: "Line: 27, Column: 48, File-Position: 772"
    QRegularExpression posRegex(
        "Line:\\s*(\\d+),\\s*Column:\\s*(\\d+)(?:,\\s*File-Position:\\s*(\\d+))?",
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch posMatch = posRegex.match(sbclOutput);
    if (posMatch.hasMatch()) {
        msg.line = posMatch.captured(1).toInt();
        msg.column = posMatch.captured(2).toInt();
        if (posMatch.lastCapturedIndex() >= 3 && !posMatch.captured(3).isEmpty()) {
            msg.filePosition = posMatch.captured(3).toInt();
        }
    }

    // ===== 2. Ищем имя файла =====
    // Формат: "file C:\\Users\\alexy\\test.lisp"
    // или: "file \"C:\\Users\\alexy\\test.lisp\""
    QRegularExpression fileRegex(
        "file\\s+[\"']?([^\"'\\n]+?\\.lisp)[\"']?",
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch fileMatch = fileRegex.match(sbclOutput);
    if (fileMatch.hasMatch()) {
        msg.file = fileMatch.captured(1).trimmed();
        // Очищаем от лишних символов
        msg.file.remove(QRegularExpression("[\\n\\r]"));
    }

    // ===== 3. Reader error (ошибка чтения) =====
    QRegularExpression readerPattern(
        "(unmatched close parenthesis|unexpected EOF.*|unexpected token.*|READ error during LOAD)",
        QRegularExpression::CaseInsensitiveOption);
    auto readerMatch = readerPattern.match(sbclOutput);
    if (readerMatch.hasMatch()) {
        msg.type = SBCLMessageType::ReaderError;
        msg.message = readerMatch.captured(0);

        // Добавляем позицию в сообщение, если есть
        if (msg.line.has_value() && msg.column.has_value()) {
            msg.message = QString("%1 at line %2, column %3")
                .arg(readerMatch.captured(0))
                .arg(*msg.line)
                .arg(*msg.column);
        }

        return msg;
    }

    // ===== 4. Runtime error (ошибка выполнения) =====
    QRegularExpression runtimePattern(
        "(invalid number of arguments:\\s*\\d+|division by zero|type error.*|illegal function call|undefined function)",
        QRegularExpression::CaseInsensitiveOption);
    auto runtimeMatch = runtimePattern.match(sbclOutput);
    if (runtimeMatch.hasMatch()) {
        msg.type = SBCLMessageType::RuntimeError;
        msg.message = runtimeMatch.captured(0);
        return msg;
    }

    // ===== 5. Undefined function =====
    QRegularExpression undefPattern(
        "undefined function:\\s*(.*)",
        QRegularExpression::CaseInsensitiveOption);
    auto undefMatch = undefPattern.match(sbclOutput);
    if (undefMatch.hasMatch()) {
        msg.type = SBCLMessageType::RuntimeError;
        msg.message = "undefined function: " + undefMatch.captured(1);
        return msg;
    }

    // ===== 6. Unbound variable =====
    QRegularExpression unboundPattern(
        "The variable\\s+(.*?)\\s+is unbound\\.",
        QRegularExpression::CaseInsensitiveOption);
    auto unboundMatch = unboundPattern.match(sbclOutput);
    if (unboundMatch.hasMatch()) {
        msg.type = SBCLMessageType::RuntimeError;
        msg.message = "unbound variable: " + unboundMatch.captured(1);
        return msg;
    }

    // ===== 7. caught ERROR (ошибка компиляции) =====
    if (sbclOutput.contains("caught ERROR:", Qt::CaseInsensitive)) {
        msg.type = SBCLMessageType::RuntimeError;

        // Извлекаем сообщение об ошибке
        QRegularExpression errorRegex("caught ERROR:\\s*\\n?\\s*([^\\n]+)");
        QRegularExpressionMatch errorMatch = errorRegex.match(sbclOutput);
        if (errorMatch.hasMatch()) {
            msg.message = errorMatch.captured(1).trimmed();
        }
        else {
            msg.message = "Compilation error";
        }

        return msg;
    }

    // ===== 8. debugger invoked (отладчик) =====
    if (sbclOutput.contains("debugger invoked", Qt::CaseInsensitive)) {
        msg.type = SBCLMessageType::RuntimeError;

        // Извлекаем сообщение об ошибке из отладчика
        QRegularExpression debuggerRegex("debugger invoked on a[^\\n]+\\n:\\s*([^\\n]+)");
        QRegularExpressionMatch debuggerMatch = debuggerRegex.match(sbclOutput);
        if (debuggerMatch.hasMatch()) {
            msg.message = debuggerMatch.captured(1).trimmed();
        }
        else {
            msg.message = "Runtime error (debugger invoked)";
        }

        return msg;
    }

    // ===== 9. Любая другая ошибка =====
    if (sbclOutput.contains("error", Qt::CaseInsensitive) ||
        sbclOutput.contains("warning", Qt::CaseInsensitive)) {
        msg.type = SBCLMessageType::RuntimeError;
        QString cleanedOutput = filterOutput(sbclOutput);
        msg.message = cleanedOutput.isEmpty() ? sbclOutput : cleanedOutput;
        return msg;
    }

    // ===== 10. Success — результат программы =====
    msg.type = SBCLMessageType::Success;
    msg.message = filterOutput(sbclOutput);
    return msg;
}