#include "repl_parser.h"

#include <QRegularExpression>
#include <QDebug>

ReplParser::ReplParser(QObject* parent)
    : QObject(parent)
{
}

void ReplParser::pushData(const QString& chunk)
{
    m_buffer += chunk;

    // Разбираем построчно
    int pos = 0;
    while ((pos = m_buffer.indexOf('\n')) != -1) {
        QString line = m_buffer.left(pos);
        m_buffer = m_buffer.mid(pos + 1);

        if (line.endsWith('\r')) {
            line.chop(1);
        }

        processLine(line);
    }
}

void ReplParser::reset()
{
    m_buffer.clear();
}

void ReplParser::setPrompt(const QString& prompt)
{
    m_currentPrompt = prompt;
}

bool ReplParser::isPrompt(const QString& line) const
{
    QString trimmed = line.trimmed();

    // SBCL стандартный промпт
    if (trimmed.startsWith("CL-USER>")) return true;

    // Промпт после in-package (любой текст, заканчивающийся на "> ")
    if (trimmed.endsWith("> ") && trimmed.contains(">")) {
        // Проверяем, что это не часть ошибки
        if (!trimmed.contains("error", Qt::CaseInsensitive) &&
            !trimmed.contains("warning", Qt::CaseInsensitive)) {
            return true;
        }
    }

    // Промпт отладчика SBCL
    if (trimmed.startsWith("debugger invoked")) return false;

    // Звёздочка как промпт (в отладчике)
    if (trimmed == "*") return true;

    return false;
}

bool ReplParser::isTechnicalLine(const QString& line) const
{
    QString trimmed = line.trimmed();

    // Служебные строки SBCL
    if (trimmed.startsWith("Unhandled")) return true;
    if (trimmed.startsWith("Backtrace")) return true;
    if (trimmed.startsWith("Stream:")) return true;
    if (trimmed.contains("foreign function")) return true;
    if (trimmed.contains("unhandled condition")) return true;

    // Номера строк в backtrace
    if (QRegularExpression("^\\d+:").match(trimmed).hasMatch()) return true;

    // Строки с фигурными скобками (технические)
    if (trimmed.contains("{") && trimmed.contains("}>")) return true;

    // Временные метки [12345]
    if (QRegularExpression("^\\[\\d+\\]").match(trimmed).hasMatch()) return true;

    return false;
}

ReplMessage ReplParser::parseError(const QString& line) const
{
    ReplMessage msg;
    msg.type = ReplMessageType::Error;
    msg.text = line.trimmed();

    // Ищем позицию ошибки: "Line: 27, Column: 48"
    QRegularExpression posRegex(
        "Line:\\s*(\\d+),\\s*Column:\\s*(\\d+)",
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = posRegex.match(line);

    if (match.hasMatch()) {
        msg.line = match.captured(1).toInt();
        msg.column = match.captured(2).toInt();
    }

    // Ищем имя файла: "file C:\\path\\file.lisp"
    QRegularExpression fileRegex(
        "file\\s+[\"']?([^\"'\\n]+\\.lisp)[\"']?",
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch fileMatch = fileRegex.match(line);

    if (fileMatch.hasMatch()) {
        msg.file = fileMatch.captured(1).trimmed();
    }

    return msg;
}

ReplMessage ReplParser::parseResult(const QString& line) const
{
    ReplMessage msg;
    msg.type = ReplMessageType::Result;

    QString trimmed = line.trimmed();

    // Убираем звёздочку в начале (результат SBCL)
    if (trimmed.startsWith("*")) {
        msg.text = trimmed.mid(1).trimmed();
    }
    else {
        msg.text = trimmed;
    }

    return msg;
}

void ReplParser::processLine(const QString& line)
{
    if (line.isEmpty()) return;

    // 1. Проверяем на промпт
    if (isPrompt(line)) {
        ReplMessage msg;
        msg.type = ReplMessageType::Prompt;
        msg.text = line.trimmed();
        emit messageReady(msg);
        return;
    }

    // 2. Проверяем на технические строки (игнорируем)
    if (isTechnicalLine(line)) {
        return;
    }

    // 3. Проверяем на ошибку с позицией
    if (line.contains("Line:") && line.contains("Column:")) {
        emit messageReady(parseError(line));
        return;
    }

    // 4. Проверяем на другие ошибки
    if (line.contains("error", Qt::CaseInsensitive) ||
        line.contains("warning", Qt::CaseInsensitive) ||
        line.contains("caught ERROR", Qt::CaseInsensitive) ||
        line.contains("unbound variable", Qt::CaseInsensitive) ||
        line.contains("undefined function", Qt::CaseInsensitive)) {

        ReplMessage msg;
        msg.type = ReplMessageType::Error;
        msg.text = line.trimmed();
        emit messageReady(msg);
        return;
    }

    // 5. Всё остальное — результат или обычный вывод
    emit messageReady(parseResult(line));
}