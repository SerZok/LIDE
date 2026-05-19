#include "repl_parser.h"

#include <QRegularExpression>
#include <QString>
#include <QDebug>

ReplParser::ReplParser(QObject* parent)
    : QObject(parent)
    , m_settings(Settings::instance())
    , m_state(ParseState::Idle)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(m_settings->replOutputDelay());
    m_maxLinesPerTick = m_settings->replChunkSize();
    m_parseMode = m_settings->replParseOutputMode();

    connect(m_settings, &Settings::replOutputTimeChanged, this, [this]() { m_timer->setInterval(m_settings->replOutputDelay()); });
    connect(m_settings, &Settings::replChunkSizeChanged, this, [this]() { m_maxLinesPerTick = m_settings->replChunkSize(); });
    connect(m_settings, &Settings::replParseModeChanged, this, [this]() { m_parseMode = m_settings->replParseOutputMode(); });

    connect(m_timer, &QTimer::timeout, this, &ReplParser::processBuffer);
    m_timer->start();
}

void ReplParser::pushData(const QString& chunk)
{
    m_inputBuffer += chunk;
}

void ReplParser::reset()
{
    m_inputBuffer.clear();
    m_blockBuffer.clear();
    m_currentPrompt.clear();
    m_state = ParseState::Idle;
}

void ReplParser::setPrompt(const QString& prompt)
{
    m_currentPrompt = prompt;
}

void ReplParser::setParserMode(int mode)
{
    m_parseMode = static_cast<Settings::ParseOutputMode>(mode);
}

void ReplParser::processBuffer()
{
    if (m_inputBuffer.isEmpty()) return;

    int processed = 0;
    int pos;

    while ((pos = m_inputBuffer.indexOf('\n')) != -1 && processed < m_maxLinesPerTick)
    {
        QString line = m_inputBuffer.left(pos);
        m_inputBuffer.remove(0, pos + 1);

        // Сырой вывод строки для анализа
        qDebug() << "[line]:" << line;

        // Проверяем промпты
        if (isPromptLine(line)) {
            if (!m_blockBuffer.isEmpty()) {
                flushBlock();
            }
            ReplMessage promptMsg;
            promptMsg.type = ReplMessageType::Prompt;
            promptMsg.text = (m_parseMode != Settings::ParseOutputMode::Full?line.trimmed():line);
            emit messageReady(promptMsg);
            m_state = ParseState::Idle;
            continue;
        }

        m_blockBuffer.append(line);

        if (m_state == ParseState::Idle) {
            if (isErrorStart(line)) {
                m_state = ParseState::CollectingError;
            }
            else if (isBacktraceStart(line)) {
                m_state = ParseState::CollectingBacktrace;
            }
        }

        if (isBlockComplete()) {
            flushBlock();
        }

        processed++;
    }

    // Обработка "хвоста"
    QString remaining = m_inputBuffer.trimmed();
    if (!remaining.isEmpty() && isPromptLine(remaining)) {
        if (!m_blockBuffer.isEmpty()) flushBlock();

        ReplMessage promptMsg;
        promptMsg.type = ReplMessageType::Prompt;
        promptMsg.text = (m_parseMode != Settings::ParseOutputMode::Full ? remaining : m_inputBuffer);
        emit messageReady(promptMsg);

        m_inputBuffer.clear();
        m_state = ParseState::Idle;
    }
}

bool ReplParser::isBlockComplete() const
{
    if (m_blockBuffer.isEmpty()) return false;

    const QString lastLine = m_blockBuffer.last().trimmed();

    if (m_state == ParseState::CollectingError || m_state == ParseState::CollectingBacktrace) {
        if (lastLine.contains("unhandled condition") && lastLine.contains("quitting")) {
            qDebug() << "[isBlockComplete] END: quitting detected";
            return true;
        }

        if (isPromptLine(lastLine)) {
            return true;
        }

        if (m_blockBuffer.size() > 10 && isErrorStart(lastLine)) {
            return true;
        }

        return false;
    }

    if (m_state == ParseState::Idle && !m_blockBuffer.isEmpty()) {
        const QString firstLine = m_blockBuffer.first().trimmed();
        if (!isErrorStart(firstLine) && !isTechnicalNoise(firstLine)) {
            return m_blockBuffer.size() >= 1;
        }
    }

    return false;
}

void ReplParser::flushBlock()
{
    if (m_blockBuffer.isEmpty()) return;

    BlockType type = classifyBlock(m_blockBuffer);

    ReplMessage msg;
    switch (type) {
    case BlockType::Error:
        msg = parseErrorBlock(m_blockBuffer);
        break;
    case BlockType::Warning:
        msg = parseWarningBlock(m_blockBuffer);
        break;
    case BlockType::Result:
        msg = parseResultBlock(m_blockBuffer);
        break;
    case BlockType::Prompt:
        msg.type = ReplMessageType::Prompt;
        msg.text = (m_parseMode != Settings::ParseOutputMode::Full ? m_blockBuffer.join("\n").trimmed() : m_blockBuffer.join("\n"));
        break;
    default:
        msg.type = ReplMessageType::Output;
        msg.text = (m_parseMode != Settings::ParseOutputMode::Full ? m_blockBuffer.join("\n").trimmed() : m_blockBuffer.join("\n"));
    }

    if (shouldEmitBlock(type, m_blockBuffer)) {
        emit messageReady(msg);
    }

    if (m_pendingSbclTerminated) {
        qDebug() << "[flushBlock] EMITTING deferred sbclTerminated";
        emit sbclTerminated();
        m_pendingSbclTerminated = false;
    }

    m_blockBuffer.clear();
    m_state = ParseState::Idle;
}

ReplParser::BlockType ReplParser::classifyBlock(const QStringList& lines) const
{
    if (lines.isEmpty()) return BlockType::TechnicalNoise;

    const QString joined = lines.join("\n");
    const QString joinedLower = joined.toLower();

    for (const QString& line : lines) {
        QString t = line.trimmed();
        if (t.isEmpty()) continue;

        if (isPromptLine(line)) return BlockType::Prompt;

        if (t.startsWith("Unhandled") ||
            t.contains("READ error") ||
            t.startsWith("debugger invoked") ||
            t.contains("unhandled condition")) {
            return BlockType::Error;
        }

        if (t.contains("Line:") && t.contains("Column:")) {
            return BlockType::Error;
        }
    }

    if (joinedLower.contains("unhandled") ||
        joinedLower.contains("read error") ||
        joinedLower.contains("compilation error") ||
        joinedLower.contains("unbound variable") ||
        joinedLower.contains("undefined function") ||
        joinedLower.contains("debugger invoked")) {
        return BlockType::Error;
    }

    if (joinedLower.contains("warning") || joinedLower.contains("caught ")) {
        return BlockType::Warning;
    }

    bool allNoise = true;
    for (const QString& line : lines) {
        if (!line.trimmed().isEmpty() && !isTechnicalNoise(line)) {
            allNoise = false;
            break;
        }
    }
    if (allNoise && !lines.isEmpty()) {
        return BlockType::TechnicalNoise;
    }

    return BlockType::Result;
}

bool ReplParser::isErrorStart(const QString& line) const
{
    const QString t = line.trimmed();

    if (t.startsWith("Unhandled") ||
        t.contains("READ error") ||
        t.startsWith("debugger invoked")) {
        return true;
    }

    if (t.startsWith(";") && (
        t.contains("undefined function", Qt::CaseInsensitive) ||
        t.contains("unbound variable", Qt::CaseInsensitive) ||
        t.contains("compilation error", Qt::CaseInsensitive) ||
        t.contains("read error", Qt::CaseInsensitive))) {
        return true;
    }

    return false;
}

bool ReplParser::isBacktraceStart(const QString& line) const
{
    return line.trimmed().startsWith("Backtrace for:");
}

bool ReplParser::isPromptLine(const QString& line) const
{
    const QString t = line.trimmed();

    static const QStringList prompts = {
        "CL-USER>", "*", "DEBUG>", "CL-USER> ", "* ", "DEBUG> "
    };
    for (const QString& p : prompts) {
        if (t == p || t.startsWith(p)) return true;
    }

    static QRegularExpression levelPrompt(R"(^\[\d+\]>\s*$)");
    static QRegularExpression sbclDebugPrompt(R"(^\d+\]\s*$)");
    static QRegularExpression pkgPrompt(R"(^[A-Z0-9\-]+>\s*$)");

    if (levelPrompt.match(t).hasMatch()) return true;
    if (sbclDebugPrompt.match(t).hasMatch()) return true;
    if (pkgPrompt.match(t).hasMatch() && !t.contains("error", Qt::CaseInsensitive)) {
        return true;
    }

    return false;
}

bool ReplParser::isTechnicalNoise(const QString& line) const
{
    const QString t = line.trimmed();
    if (QRegularExpression(R"(^\[\d+\]\s*$)").match(t).hasMatch()) return true;
    return false;
}

ReplMessage ReplParser::parseErrorBlock(const QStringList& lines)
{
    ReplMessage msg;
    msg.type = ReplMessageType::Error;

    const QString joined = lines.join("\n");

    extractErrorPosition(joined, msg);
    extractErrorFile(joined, msg);

    if (joined.contains("unhandled condition in --disable-debugger mode, quitting", Qt::CaseInsensitive)) {
        qDebug() << "[parseErrorBlock] SBCL terminated signal detected";
        m_pendingSbclTerminated = true;
    }

    msg.text = formatErrorText(lines, m_parseMode);

    if (msg.line.has_value() && msg.column.has_value()) {
        if(m_parseMode == Settings::ParseOutputMode::Full)
            emit errorLocationAvailable(formatErrorText(lines, Settings::ParseOutputMode::Minimal), msg.line.value(), msg.column.value());
        else
            emit errorLocationAvailable(msg.text, msg.line.value(), msg.column.value());
    }

    return msg;
}

ReplMessage ReplParser::parseWarningBlock(const QStringList& lines)
{
    ReplMessage msg;
    msg.type = ReplMessageType::Warning;

    if (m_parseMode == Settings::ParseOutputMode::Minimal) {
        for (const QString& line : lines) {
            QString t = line.trimmed();
            if (!t.isEmpty() && t.toLower().contains("warning")) {
                msg.text = t;
                return msg;
            }
        }
    }

    msg.text = (m_parseMode != Settings::ParseOutputMode::Full ? lines.join("\n").trimmed() : lines.join("\n"));
    return msg;
}

ReplMessage ReplParser::parseResultBlock(const QStringList& lines)
{
    ReplMessage msg;
    msg.type = ReplMessageType::Result;

    QString line = lines.join("\n");

    QString text = line.trimmed();

    if (text.startsWith("*")) {
        text = text.mid(1).trimmed();
    }

    msg.text = (m_parseMode != Settings::ParseOutputMode::Full ? text : line);
    return msg;
}

bool ReplParser::tryExtractPosition(const QString& text, int& line, int& column) const
{
    static const QList<QRegularExpression> patterns = {
        QRegularExpression(R"(Line:\s*(\d+),\s*Column:\s*(\d+))", QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(R"(line\s+(\d+),\s*column\s+(\d+))", QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(R"(starting at line\s+(\d+),\s*column\s+(\d+))", QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(R"(at\s+line\s+(\d+)\s+column\s+(\d+))", QRegularExpression::CaseInsensitiveOption)
    };

    for (const auto& re : patterns) {
        auto match = re.match(text);
        if (match.hasMatch()) {
            line = match.captured(1).toInt();
            column = match.captured(2).toInt();
            return true;
        }
    }
    return false;
}

void ReplParser::extractErrorPosition(const QString& joined, ReplMessage& msg) const
{
    int line, column;
    if (tryExtractPosition(joined, line, column)) {
        msg.line = line;
        msg.column = column;
        qDebug() << "[extractErrorPosition] FOUND" << line << ":" << column;
    }
}

void ReplParser::extractErrorFile(const QString& joined, ReplMessage& msg) const
{
    static const QRegularExpression fileRegex1(R"(for\s+"file\s+([^"]+))", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression fileRegex2(R"pat(of\s+#P"([^"]+)")pat");

    QString extracted;

    auto match = fileRegex1.match(joined);
    if (match.hasMatch()) extracted = match.captured(1);

    if (extracted.isEmpty()) {
        match = fileRegex2.match(joined);
        if (match.hasMatch()) extracted = match.captured(1);
    }

    if (!extracted.isEmpty()) {
        if (extracted.endsWith(".lisp", Qt::CaseInsensitive) ||
            extracted.endsWith(".lsp", Qt::CaseInsensitive) ||
            extracted.endsWith(".cl", Qt::CaseInsensitive) ||
            extracted.endsWith(".l", Qt::CaseInsensitive) ||
            extracted.endsWith(".fasl", Qt::CaseInsensitive)) {

            msg.file = extracted.trimmed().replace("\\\\", "\\");
            qDebug() << "[extractErrorFile] FOUND:" << msg.file.value();
        }
    }
}

QString ReplParser::formatErrorText(const QStringList& lines, Settings::ParseOutputMode mode) const
{
    QStringList result;

    for (const QString& rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty() && mode != Settings::ParseOutputMode::Full) continue;

        switch (mode) {
        case Settings::ParseOutputMode::Minimal:
        {
            if (line.contains("READ error during LOAD:", Qt::CaseInsensitive)) {
                result.append(line);
                continue;
            }

            if (line.contains("Line:", Qt::CaseInsensitive) && line.contains("Column:", Qt::CaseInsensitive)) {
                result.append(line);
                continue;
            }

            if (line.contains("starting at line", Qt::CaseInsensitive) && line.contains("column", Qt::CaseInsensitive)) {
                result.append(line);
                continue;
            }

            if (line.contains("unmatched") || line.contains("unexpected") ||
                line.contains("reader error") || line.contains("syntax error") ||
                line.contains("undefined function") || line.contains("unbound variable")) {

                if (!line.contains("#<") &&
                    !QRegularExpression(R"(\{[0-9A-F]+\})").match(line).hasMatch() &&
                    !QRegularExpression(R"(^\d+:)").match(line).hasMatch() &&
                    !line.contains("for \"file") &&
                    !line.contains("SB-C::") &&
                    line.length() < 100) {
                    result.append(line);
                }
                continue;
            }

            if (line.startsWith("of #P\"") || line == "of") {
                continue;
            }

            continue;
        }

        case Settings::ParseOutputMode::Simple:
        {
            if (line.contains("READ error during LOAD:", Qt::CaseInsensitive) ||
                (line.contains("Line:", Qt::CaseInsensitive) && line.contains("Column:", Qt::CaseInsensitive)) ||
                (line.contains("starting at line", Qt::CaseInsensitive) && line.contains("column", Qt::CaseInsensitive))) {
                result.append(line);
                continue;
            }

            if (line.contains("unmatched") || line.contains("unexpected") ||
                line.contains("reader error") || line.contains("syntax error") ||
                line.contains("undefined function") || line.contains("unbound variable")) {

                if (!line.contains("#<") &&
                    !QRegularExpression(R"(\{[0-9A-F]+\})").match(line).hasMatch() &&
                    !QRegularExpression(R"(^\d+:)").match(line).hasMatch() &&
                    !line.contains("for \"file") &&
                    !line.contains("SB-C::") &&
                    line.length() < 100) {
                    result.append(line);
                }
                continue;
            }

            if (line.startsWith("of #P\"") || line == "of") {
                continue;
            }

            continue;
        }

        case Settings::ParseOutputMode::Full:
        default:
        {
            result.append(rawLine);
            break;
        }
        }
    }

    return result.join("\n");
}

QString ReplParser::shortenBacktraceFrame(const QString& frame) const
{
    static QRegularExpression frameRegex(R"(^\s*(\d+):\s+\(([^)\s]+))");
    auto match = frameRegex.match(frame);

    if (match.hasMatch()) {
        return QString("%1: %2(...)").arg(match.captured(1), match.captured(2));
    }
    return frame;
}

bool ReplParser::shouldEmitBlock(BlockType type, const QStringList& lines) const
{
    if (type == BlockType::Prompt) return true;
    if (type == BlockType::Error) return true;

    if (type == BlockType::Warning) {
        if (m_parseMode == Settings::ParseOutputMode::Minimal) {
            for (const QString& line : lines) {
                QString t = line.trimmed();
                if (t.isEmpty() || t.startsWith(";")) {
                    static const QStringList critical = {
                        "error", "unbound variable", "undefined function", "compilation error"
                    };
                    for (const QString& kw : critical) {
                        if (t.contains(kw, Qt::CaseInsensitive)) return true;
                    }
                    return false;
                }
            }
        }
        return true;
    }

    if (type == BlockType::TechnicalNoise) {
        return m_parseMode == Settings::ParseOutputMode::Full;
    }

    if (m_parseMode == Settings::ParseOutputMode::Minimal) {
        bool hasContent = false;
        for (const QString& line : lines) {
            QString t = line.trimmed();
            if (!t.isEmpty() && !t.startsWith(";") && !t.startsWith("of #P") && t != "of") {
                hasContent = true;
                break;
            }
        }
        return hasContent;
    }

    if (m_parseMode == Settings::ParseOutputMode::Simple) {
        for (const QString& line : lines) {
            QString t = line.trimmed();
            if (t.isEmpty()) continue;
            if (shouldFilterComment(line)) continue;
            return true;
        }
        return false;
    }

    return true;
}

bool ReplParser::shouldFilterComment(const QString& line) const
{
    QString trimmed = line.trimmed();
    if (!trimmed.startsWith(";")) return false;

    QString content = trimmed.mid(1).trimmed();

    static const QStringList important = {
        "caught warning", "caught error",
        "undefined variable", "unbound variable",
        "undefined function", "compilation error", "error"
    };
    for (const QString& kw : important) {
        if (content.contains(kw, Qt::CaseInsensitive)) return false;
    }

    if (m_parseMode == Settings::ParseOutputMode::Simple) {
        static const QStringList noise = {
            "compilation unit finished", "in:", "note:"
        };
        for (const QString& kw : noise) {
            if (content.startsWith(kw, Qt::CaseInsensitive)) return true;
        }
    }

    return false;
}