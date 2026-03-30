#include "console.h"

void Console::appendOutput(const QString& text, bool isError, bool isNotice)
{
	QTextCursor cursor = textCursor();
	cursor.setPosition(m_editableStart);
	QString currentCommandLine = getCurrentCommandLineText();
	if (isNotice)
	{
		// Добавляем команду в историю
		if (!currentCommandLine.isEmpty()) {
			m_commandHistory.append(currentCommandLine);
			m_historyIndex = m_commandHistory.size();
		}
		//Удаляем строку
		cursor.movePosition(QTextCursor::StartOfBlock);
		cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
		cursor.removeSelectedText();
		m_editableStart = cursor.position();
	}
	// Сохраняем позицию курсора для восстановления (если пользователь редактировал)
	QTextCursor userCursor = textCursor();
	int userPos = userCursor.position();

	// Вставляем вывод в конец документа
	moveCursor(QTextCursor::End);
	if (isError) {
		setTextColor(Qt::red);
	}
	else {
		setTextColor(Qt::white);
	}
	insertPlainText(text);

	if (isNotice)
	{
		m_waitingForInput = true;
		appendPrompt();
		if (!currentCommandLine.isEmpty())
			insertFromHistory(-1);
	}

	// После добавления вывода editableStart смещается на новый конец,
	// но prompt не добавляется автоматически здесь — это делает appendPrompt().
	// Восстанавливаем курсор пользователя, если он был в editable-области.
	if (userPos > m_editableStart) {
		userCursor.setPosition(userPos);
		setTextCursor(userCursor);
	}
	else {
		// Если курсор был в истории — держим курсор в editable-области
		moveCursor(QTextCursor::End);
	}
}

void Console::appendPrompt(bool force)
{
	if (!m_waitingForInput) return;

	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End);

	// Проверяем, пустой ли документ
	if (document()->isEmpty()) {
		// Если документ пустой, просто вставляем промпт
		QTextCharFormat promptFormat;
		promptFormat.setForeground(Qt::green);
		cursor.insertText(m_prompt, promptFormat);

		cursor.movePosition(QTextCursor::End);
		m_editableStart = cursor.position();
		setTextCursor(cursor);
		setTextColor(Qt::white);

		document()->clearUndoRedoStacks(QTextDocument::UndoStack);
		document()->clearUndoRedoStacks(QTextDocument::RedoStack);
		qDebug() << "Пустой документ, m_editableStart:" << m_editableStart;
		return;
	}

	// Проверяем последнюю строку
	cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
	QString lastLine = cursor.selectedText();
	cursor.clearSelection();

	// Если промпт уже есть, обновляем позицию и выходим
	if (lastLine == m_prompt && !force) {
		cursor.movePosition(QTextCursor::End);
		m_editableStart = cursor.position();
		qDebug() << "Промпт уже существует, m_editableStart:" << m_editableStart;
		// Убеждаемся, что цвет для ввода белый
		setTextColor(Qt::white);
		return;
	}

	// Если последняя строка не пустая, добавляем перевод строки
	if (!lastLine.isEmpty()) {
		cursor.movePosition(QTextCursor::End);
		cursor.insertText("\n");
	}

	// Создаем формат для промпта
	QTextCharFormat promptFormat;
	promptFormat.setForeground(Qt::green);

	// Вставляем промпт с зеленым цветом
	cursor.insertText(m_prompt, promptFormat);

	// Обновляем позицию редактирования (после промпта)
	cursor.movePosition(QTextCursor::End);
	m_editableStart = cursor.position();

	// Устанавливаем курсор и убеждаемся, что цвет для ввода белый
	setTextCursor(cursor);
	setTextColor(Qt::white);

	document()->clearUndoRedoStacks(QTextDocument::UndoStack);
	document()->clearUndoRedoStacks(QTextDocument::RedoStack);
}
