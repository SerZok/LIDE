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
	parseMode = m_settings->replParseOutputMode();

	connect(m_settings, &Settings::replOutputTimeChanged, this, [this]() {m_timer->setInterval(m_settings->replOutputDelay()); });
	connect(m_settings, &Settings::replChunkSizeChanged, this, [this]() {m_maxLinesPerTick = m_settings->replChunkSize(); });
	connect(m_settings, &Settings::replParseModeChanged, this, [this]() {parseMode = m_settings->replParseOutputMode(); });

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
	const QString t = line.trimmed();

	// Стандартные промпты
	static const QStringList prompts = {
		"CL-USER>", "CL-USER> ",
		"*", "* ",
		"DEBUG>", "DEBUG> "
	};
	for (const QString& p : prompts) {
		if (t == p || t.startsWith(p)) return true;
	}

	// Промпты с номером уровня: [1], [2]> и т.д.
	static QRegularExpression levelPrompt(R"(^\[\d+\]>\s*$)");
	if (levelPrompt.match(t).hasMatch()) return true;

	// Промпты с номером уровня : [1] , [2] без ">"
	static QRegularExpression sbclDebugPrompt(R"(^\d+\]\s*$)");
	if (sbclDebugPrompt.match(t).hasMatch()) return true;

	// Package-qualified prompts: MY-PACKAGE>
	static QRegularExpression pkgPrompt(R"(^[A-Z0-9\-]+>\s*$)");
	if (pkgPrompt.match(t).hasMatch() && !t.contains("error", Qt::CaseInsensitive)) {
		return true;
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
	if (parseMode == Settings::ParseOutputMode::Minimal) {
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
	if (parseMode == Settings::ParseOutputMode::Simple) {
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

	if (parseMode == Settings::ParseOutputMode::Full) {
		return !content.contains("error", Qt::CaseInsensitive);
	}

	return false;
}

ReplMessage ReplParser::parseError(const QString& line)
{
	ReplMessage msg;
	msg.type = ReplMessageType::Error;
	msg.text = line.trimmed();

	static const QRegularExpression posRegex(
		"Line:\\s*(\\d+),\\s*Column:\\s*(\\d+)",
		QRegularExpression::CaseInsensitiveOption);

	static const QRegularExpression evalRegex(
		"line\\s*(\\d+),\\s*column\\s*(\\d+)",
		QRegularExpression::CaseInsensitiveOption);

	static const QRegularExpression fileRegex(
		"file\\s+[\"']?([^\"'\\n]+\\.lisp)[\"']?|#P\"([^\"]+\\.lisp)\"",
		QRegularExpression::CaseInsensitiveOption);

	// ищем координаты
	QRegularExpressionMatch match = posRegex.match(line);

	if (!match.hasMatch())
		match = evalRegex.match(line);

	if (match.hasMatch())
	{
		msg.line = match.captured(1).toInt();
		msg.column = match.captured(2).toInt();
	}

	// ищем файл
	QRegularExpressionMatch fileMatch = fileRegex.match(line);

	if (fileMatch.hasMatch())
	{
		QString file = fileMatch.captured(1);
		if (file.isEmpty())
			file = fileMatch.captured(2);

		msg.file = file.trimmed();
	}

	if (msg.line.has_value() && msg.column.has_value())
	{
		emit errorLocationAvailable(msg.text, msg.line.value(), msg.column.value());
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

bool ReplParser::isTechnicalOrFiltered(const QString& line) const
{

	const QString trimmed = line.trimmed();

	// Промпты никогда не фильтруем
	if (isPrompt(line)) return false;

	// Полноэкранный режим — показываем почти всё
	//if (parseMode == Settings::ParseOutputMode::Full) {
	//	return isTechnicalLine(trimmed); // только совсем мусор
	//}

	// Минимальный режим — жёсткая фильтрация
	if (parseMode == Settings::ParseOutputMode::Minimal) {
		if (isTechnicalLine(trimmed)) return true;
		if (trimmed.startsWith(";")) {
			// Показываем комментарии только если там есть "error"
			return !trimmed.contains("error", Qt::CaseInsensitive);
		}
		return false;
	}

	// Нормальный режим (по умолчанию)
	if (isTechnicalLine(trimmed)) return true;

	// Комментарии: скрываем только "информационные" от компилятора
	if (trimmed.startsWith(";")) {
		static const QStringList noise = {
			"compilation unit finished",
			"in:", "note:", "debugger invoked"
		};
		for (const QString& kw : noise) {
			if (trimmed.contains(kw, Qt::CaseInsensitive))
				return true;
		}
	}

	return false;
}

ReplMessage ReplParser::parseLineType(const QString& line)
{
	QString trimmed = line.trimmed();
	QString lower = trimmed.toLower();

	if (isPrompt(line))
		return { ReplMessageType::Prompt, trimmed };

	if (trimmed.contains("Line:") && trimmed.contains("Column:"))
		return parseError(line);

	if (lower.contains("line ") && lower.contains("column "))
		return parseError(line);

	bool isError =
		lower.contains("error") ||
		lower.contains("unbound variable") ||
		lower.contains("undefined function") ||
		lower.contains("compilation error") ||
		lower.contains("read error");

	bool isWarning =
		lower.contains("warning") ||
		lower.contains("caught");

	switch (parseMode)
	{

		case Settings::ParseOutputMode::Minimal:
		{
			if (isError)
				return parseError(line);

			return parseResult(line);
		}

		case Settings::ParseOutputMode::Simple:
		{
			if (isError)
				return parseError(line);

			if (isWarning)
			{
				ReplMessage msg;
				msg.type = ReplMessageType::Warning;
				msg.text = trimmed;
				return msg;
			}

			return parseResult(line);
		}

		case Settings::ParseOutputMode::Full:
		{
			if (isError)
				return parseError(line);

			if (isWarning)
			{
				ReplMessage msg;
				msg.type = ReplMessageType::Warning;
				msg.text = trimmed;
				return msg;
			}

			return parseResult(line);
		}
	}

	return parseResult(line);
}

void ReplParser::processLine(const QString& line)
{
	qDebug() << "[rawLine]: " << line;

	if (line.trimmed().isEmpty() &&
		parseMode != Settings::ParseOutputMode::Full)
		return;

	// Не фильтруем для FULL
	if (parseMode != Settings::ParseOutputMode::Full && isTechnicalOrFiltered(line))
		return;

	emit messageReady(parseLineType(line));
}