#include "console.h"

QString Console::getCurrentCommandLineText() const
{
	// Защита от выхода за границы
	if (m_editableStart < 0 || m_editableStart > document()->characterCount()) {
		qDebug() << "Ошибка: m_editableStart вне диапазона:" << m_editableStart;
		return QString();
	}

	QTextCursor cursor = textCursor();
	cursor.setPosition(m_editableStart);
	cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
	QString text = cursor.selectedText();

	text.replace(QChar(0x2028), "\n");  // LINE SEPARATOR
	text.replace(QChar(0x2029), "\n");  // PARAGRAPH SEPARATOR
	text.replace("\r\n", "\n");         // Windows line endings
	text.replace("\r", "\n");           // Old Mac line endings

	qDebug() << "text:" << text;
	return text.trimmed();
}

void Console::insertFromHistory(int direction)
{
	if (m_commandHistory.isEmpty()) return;

	// Защита от выхода за границы
	if (m_editableStart < 0 || m_editableStart > document()->characterCount()) {
		qDebug() << "Ошибка: m_editableStart вне диапазона:" << m_editableStart;
		appendPrompt();
		return;
	}

	if (m_historyIndex < 0) m_historyIndex = m_commandHistory.size();
	m_historyIndex += direction;
	m_historyIndex = qBound(0, m_historyIndex, m_commandHistory.size() - 1);

	QString command = m_commandHistory[m_historyIndex];

	QTextCursor cursor = textCursor();
	cursor.setPosition(m_editableStart);
	cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
	cursor.removeSelectedText();

	QTextCharFormat format;
	format.setForeground(Qt::white);
	cursor.setCharFormat(format);

	cursor.insertText(command);
	cursor.setPosition(m_editableStart + command.length());
	setTextCursor(cursor);
}

void Console::ensureCursorInEditableArea()
{
	// Защита от выхода за границы
	if (m_editableStart < 0 || m_editableStart > document()->characterCount()) {
		qDebug() << "Ошибка: m_editableStart вне диапазона:" << m_editableStart;
		appendPrompt();
		return;
	}

	QTextCursor cur = textCursor();
	if (cur.position() < m_editableStart) {
		cur.setPosition(m_editableStart);
		setTextCursor(cur);
	}
}

void Console::clear()
{
	QTextEdit::clear();
	m_editableStart = 0;
	m_waitingForInput = true;
}