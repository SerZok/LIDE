#include "repl_parser.h"

#include <QRegularExpression>
#include <QString>
#include <QDebug>

ReplParser::ReplParser(QObject* parent)
    : QObject(parent)
    , m_settings(Settings::instance())
{
    m_timer = new QTimer(this);
    m_timer->setInterval(m_settings->replOutputDelay()); // обработка каждые X мс
    m_maxLinesPerTick = m_settings->replChunkSize();
    parseMode = m_settings->replParseMode();

    connect(m_settings, &Settings::replOutputTimeChanged, this, [this]() {m_timer->setInterval(m_settings->replOutputDelay()); });
    connect(m_settings, &Settings::replChunkSizeChanged, this, [this]() {m_maxLinesPerTick = m_settings->replChunkSize(); });
    connect(m_settings, &Settings::replParseModeChanged, this, [this]() {parseMode = m_settings->replParseMode(); });

    connect(m_timer, &QTimer::timeout, this, &ReplParser::processBuffer);
    m_timer->start();
}

void ReplParser::processBuffer() {
    if (m_buffer.isEmpty()) return;

    int processed = 0;
    int pos;
    while ((pos = m_buffer.indexOf('\n')) != -1 && processed < m_maxLinesPerTick) {
        QString line = m_buffer.left(pos);
        m_buffer.remove(0, pos + 1);
        processLine(line);
        processed++;
    }

    QString remaining = m_buffer.trimmed();
    if (!remaining.isEmpty() && isPrompt(remaining)) {
        processLine(remaining);
        m_buffer.clear();
    }
}

void ReplParser::pushData(const QString& chunk)
{
    m_buffer += chunk;
}

void ReplParser::reset()
{
    m_buffer.clear();
    m_currentPrompt.clear();
}

void ReplParser::setPrompt(const QString& prompt)
{
    m_currentPrompt = prompt;
}

bool ReplParser::isPrompt(const QString& line) const
{
    QString t = line.trimmed();

    if (t.startsWith("CL-USER>"))
        return true;

    if (t == "*")
        return true;

    static QRegularExpression dbgPrompt("^\\d+\\]$");
    if (dbgPrompt.match(t).hasMatch())
        return true;

    if (t.endsWith("> ") && t.contains(">")) {
        // Проверяем, что это не часть ошибки
        if (!t.contains("error", Qt::CaseInsensitive) &&
            !t.contains("warning", Qt::CaseInsensitive)) {
            return true;
        }
    }

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

bool ReplParser::isStarValue(const QString& line) const
{
    return QRegularExpression("^\\*\\s+").match(line).hasMatch();
}

bool ReplParser::shouldFilterCommentLine(const QString& line) const
{
    if (parseMode == Settings::ParseMode::Minimal) {
        return false;
    }

    QString trimmed = line.trimmed();
    if (!trimmed.startsWith(";")) return false;

    QString content = trimmed.mid(1).trimmed();

    static const QStringList important = {
        "caught warning", "caught error",
        "undefined variable", "unbound variable",
        "undefined function", "compilation error"
    };
    for (const QString& kw : important) {
        if (content.contains(kw, Qt::CaseInsensitive)) {
            return false;
        }
    }

    // В режиме Simple — фильтруем только "пустые" комментарии
    if (parseMode == Settings::ParseMode::Simple) {
        static const QStringList technical = {
            "compilation unit finished",
            "in:",  // "; in: SETQ X"
        };
        for (const QString& kw : technical) {
            if (content.startsWith(kw, Qt::CaseInsensitive)) {
                return true;
            }
        }

        // Фильтруем короткие имена с большим отступом: ";     X"
        if (trimmed.count(';') == 1 &&
            trimmed.indexOf(';') == 0 &&
            trimmed.mid(1).count(' ') >= 3 &&
            content.trimmed().length() <= 5 &&
            !content.contains(':') &&
            !content.contains(' ')) {
            return true;
        }

        // Остальные комментарии показываем
        return false;
    }

    if (parseMode == Settings::ParseMode::Full) {
        return !content.contains("error", Qt::CaseInsensitive);
    }

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
    QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return;

    QString lower = trimmed.toLower();
    bool isError = lower.contains("error") ||
        lower.contains("unbound variable") ||
        lower.contains("undefined function") ||
        lower.contains("compilation error") ||
        lower.contains("read error");

    bool isWarning = lower.contains("warning") ||
        lower.contains("undefined variable") ||
        lower.contains("caught");

    // Промпт обрабатываем всегда
    if (isPrompt(line)) {
        ReplMessage msg;
        msg.type = ReplMessageType::Prompt;
        msg.text = trimmed;
        emit messageReady(msg);
        return;
    }

    // --- Minimal ---
    if (parseMode == Settings::ParseMode::Minimal) {
        if (!isStarValue(line))
            emit messageReady(parseResult(line));
        return;
    }

    // --- Simple ---
    if (parseMode == Settings::ParseMode::Simple) {
        if (shouldFilterCommentLine(line) || isTechnicalLine(line))
            return;

        if (isError || isWarning) {
            ReplMessage msg;
            msg.type = isWarning ? ReplMessageType::Warning : ReplMessageType::Error;
            msg.text = trimmed;
            emit messageReady(msg);
            return;
        }

        if (!isStarValue(line))
            emit messageReady(parseResult(line));

        return;
    }

    // --- Full ---
    if (parseMode == Settings::ParseMode::Full) {
        // auto-restart на unhandled condition
        if (lower.contains("unhandled condition in") && lower.contains("disable-debugger")) {
            ReplMessage msg;
            msg.type = ReplMessageType::Error;
            msg.text = trimmed;
            emit messageReady(msg);
            return;
        }

        if (shouldFilterCommentLine(line) || isTechnicalLine(line))
            return;

        if (line.contains("Line:") && line.contains("Column:")) {
            emit messageReady(parseError(line));
            return;
        }

        if (isError || isWarning) {
            ReplMessage msg;
            msg.type = isWarning ? ReplMessageType::Warning : ReplMessageType::Error;
            msg.text = trimmed;
            emit messageReady(msg);
            return;
        }

        if (!isStarValue(line))
            emit messageReady(parseResult(line));
    }
}