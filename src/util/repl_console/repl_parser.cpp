#include "repl_parser.h"

#include <QRegularExpression>
#include <QString>
#include <QDebug>

ReplParser::ReplParser(QObject* parent)
    : QObject(parent)
    , m_debugMode(false)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(25); // обработка каждые 25 мс
    connect(m_timer, &QTimer::timeout, this, &ReplParser::processBuffer);
    m_timer->start();
}

void ReplParser::processBuffer() {
    if (m_buffer.isEmpty()) return;

    int processed = 0;
    int pos;
    while ((pos = m_buffer.indexOf('\n')) != -1 && processed < maxLinesPerTick) {
        QString line = m_buffer.left(pos);
        m_buffer.remove(0, pos + 1);
        processLine(line);
        processed++;
    }

    QString remaining = m_buffer.trimmed();
    if (!remaining.isEmpty() && remaining == "*") {
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

void ReplParser::setDebugMode(bool enabled)
{
    if (m_debugMode == enabled) return;
    m_debugMode = enabled;
    qDebug() << "Parser debug mode:" << (enabled ? "ON" : "OFF");
}

bool ReplParser::debugMode() const
{
    return m_debugMode;
}

bool ReplParser::isPrompt(const QString& line) const
{
    QString t = line.trimmed();

    if (t.startsWith("CL-USER>"))
        return true;

    if (t == "*")
        return true;

    if (t.endsWith("> ") && t.contains(">")) {
        // Проверяем, что это не часть ошибки
        if (!t.contains("error", Qt::CaseInsensitive) &&
            !t.contains("warning", Qt::CaseInsensitive)) {
            return true;
        }
    }
    
    // Промпт отладчика SBCL
    if (t.startsWith("debugger invoked")) return false;

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
    if (m_debugMode) return false;

    QString trimmed = line.trimmed();
    if (!trimmed.startsWith(";")) return false;

    QString content = trimmed.mid(1).trimmed();

    // Заголовок без имени: "Undefined variable:" (пусто после :)
    if (content.startsWith("undefined variable:", Qt::CaseInsensitive)) {
        QString after = content.mid(19).trimmed();
        if (after.isEmpty() || after == ":") {
            return true;
        }
    }

    // Технические маркеры
    static const QStringList filterKeywords = {
        "compilation unit finished",
        "in:",  // "; in: SETQ X"
    };
    for (const QString& kw : filterKeywords) {
        if (content.startsWith(kw, Qt::CaseInsensitive)) {
            return true;
        }
    }

    // Сильно отступленные короткие имена: ";     X"
    if (trimmed.count(';') == 1 &&  // только один ";"
        trimmed.indexOf(';') == 0 &&
        trimmed.mid(1).count(' ') >= 3 &&  // 3+ пробела после ";"
        content.trimmed().length() <= 5 &&  // короткое имя
        !content.contains(':') &&  // нет двоеточия
        !content.contains(' ')) {  // нет пробелов внутри
        return true;
    }

    // === ПОКАЗЫВАЕМ: важные строки ===
    static const QStringList showKeywords = {
        "caught warning", "caught error",
        "undefined variable", "unbound variable",
        "undefined function", "compilation error"
    };
    for (const QString& keyword : showKeywords) {
        if (content.contains(keyword, Qt::CaseInsensitive)) {
            return false;
        }
    }

    // Остальное с ";" — фильтруем
    return true;
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
    qDebug() << "Парсим строку: " << line;
    if (line.trimmed().isEmpty())
        return;

    QString lower = line.toLower();
    bool isError = lower.contains("error") ||
        lower.contains("unbound variable") ||
        lower.contains("undefined function") ||
        lower.contains("compilation error") ||
        lower.contains("read error");

    bool isWarning = lower.contains("warning") ||
        lower.contains("undefined variable") ||
        lower.contains("caught");  

    // Коментарии
    if (shouldFilterCommentLine(line)) {
        return;
    }

    // Промпт
    if (isPrompt(line)) {
        qDebug() << "Это prompt:" << line;
        ReplMessage msg;
        msg.type = ReplMessageType::Prompt;
        msg.text = line.trimmed();
        emit messageReady(msg);
        return;
    }

    // Технические строки
    if (isTechnicalLine(line)) {
        return;
    }

    // Ошибка с позицией
    if (line.contains("Line:") && line.contains("Column:")) {
        emit messageReady(parseError(line));
        return;
    }

    if (isError || isWarning) {
        ReplMessage msg;
        msg.type = isWarning ? ReplMessageType::Warning : ReplMessageType::Error;

        QString displayText = line.trimmed();
        if (!m_debugMode && displayText.startsWith(";")) {
            displayText.remove(0, 1);
            displayText = displayText.trimmed();
            if (displayText.startsWith("  ")) displayText = displayText.mid(2);
        }
        msg.text = displayText;

        emit messageReady(msg);
        return;
    }

    // * - результат
    if (isStarValue(line))
        return;

    // Обычный результат
    emit messageReady(parseResult(line));
}