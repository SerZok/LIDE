#include "console_widget.h"

#include <QScrollBar>
#include <QAction>
#include <QTextStream>
#include <QFileInfo>
#include <QThread>
#include <QMenu>
#include <QClipboard>
#include <QApplication>

Console::Console(QWidget* parent)
	: QTextEdit(parent)
	, m_historyIndex(-1)
	, m_prompt("CL-USER> ")
	, m_console_output_mark("SBCL: ")
	, m_waitingForInput(true)
	, m_editableStart(0)
	, m_last_output("")
{
	setupConsole();
	m_console_process = new ConsoleProcess();
	// Подключаем сигналы ConsoleProcess -> Console
	connect(m_console_process, &ConsoleProcess::rawOutput, this, &Console::appendOutput);
	connect(m_console_process, &ConsoleProcess::userPrompt, this, &Console::appendPrompt);
	connect(m_console_process, &ConsoleProcess::errorOccurred, this, [this](const QString& file, const QString& message, int line, int column) {
		emit errorOccurred(file, message, line, column);
		});

	// sbclMessage можно использовать для дополнительной логики (открыть файл, подсветить строку и т.п.)
	connect(m_console_process, &ConsoleProcess::sbclMessage, this, [this](const SBCLMessage& msg) {
		// Пример: при ошибке дополнительно вывести информацию о файле
		if (msg.type != SBCLMessageType::Success) {
			if (!msg.file.isEmpty())
				appendOutput(tr("Файл: %1\n").arg(msg.file), true);
			if (msg.type == SBCLMessageType::ReaderError)
				appendOutput(tr("Ошибка ридера!\n"), true);
			if (msg.type == SBCLMessageType::RuntimeError)
				appendOutput(tr("Ошибка при выполнении!\n"), true);
			if (msg.type == SBCLMessageType::Unknown)
				appendOutput(tr("Неизвестная ошибка!\n"), true);
			if (msg.line.has_value())
				appendOutput(tr("Строка: %1\n").arg(*msg.line), true);
			if (msg.column.has_value())
				appendOutput(tr("Позиция: %1\n").arg(*msg.column), true);
		}
		});

	connect(m_console_process, &ConsoleProcess::recievedNewOutputMessage, this, [this](const QString& msg) {
		m_last_output = msg;
		});

	connect(m_console_process, &ConsoleProcess::sbclPrompt, this, [this]() {
		// Создаем формат для промпта
		QTextCharFormat promptFormat;
		promptFormat.setForeground(Qt::cyan);

		QTextCursor cursor = textCursor();
		cursor.movePosition(QTextCursor::End);

		// Вставляем промпт с зеленым цветом
		cursor.insertText(m_console_output_mark, promptFormat);

		// Обновляем позицию редактирования (после промпта)
		cursor.movePosition(QTextCursor::End);

		// Устанавливаем курсор и убеждаемся, что цвет для ввода белый
		setTextCursor(cursor);
		setTextColor(Qt::white);

		appendOutput("\n", false);
		});

	appendPrompt();
	m_waitingForInput = true;
}

void Console::setupConsole()
{
	setReadOnly(false);
	setLineWrapMode(QTextEdit::NoWrap);

	// Начальный промпт
	//appendPrompt();

	// Установка objectName для CSS
	setObjectName("Console");

	// История команд
	m_commandHistory.clear();
}
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

void Console::keyPressEvent(QKeyEvent* event)
{
	// Обрабатываем сочетания Ctrl+C — посылаем сигнал kill (на Windows нестандартно)
	if (event->key() == Qt::Key_C && event->modifiers() == Qt::ControlModifier)
	{
		if (textCursor().hasSelection()) {
			copy(); // стандартное копирование
		}
		else {
			if (m_console_process) {
				m_console_process->sendCommand("(sb-ext:quit)\n");
			}
		}
		return;
	}

	// Обрабатываем сочетания Ctrl+X — вырезаем если область разрешённая
	if (event->key() == Qt::Key_X && event->modifiers() == Qt::ControlModifier)
	{
		QTextCursor cursor = textCursor();

		if (!cursor.hasSelection())
			return;

		int start = cursor.selectionStart();
		int end = cursor.selectionEnd();
		if (start >= m_editableStart && end >= m_editableStart)
		{
			QTextEdit::keyPressEvent(event);
		}
		return;
	}

	// Обрабатываем сочетания Ctrl+A — выделить весь введёнынй текст
	if (event->key() == Qt::Key_A && event->modifiers() == Qt::ControlModifier)
	{
		QTextCursor cursor = textCursor();
		cursor.setPosition(m_editableStart);
		cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		setTextCursor(cursor);
		return;
	}

	// Обрабатываем сочетания Ctrl+V — вставить
	if (event->key() == Qt::Key_V && event->modifiers() == Qt::ControlModifier)
	{
		QTextCursor cursor = textCursor();
		if (cursor.position() < m_editableStart)
			return;
		setTextColor(Qt::white);
		QTextEdit::keyPressEvent(event);
		return;
	}

	// Обрабатываем сочетания Shift+Enter — ставим перевод строки
	if ((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
		&& event->modifiers() == Qt::ShiftModifier)
	{
		// Вставляем перевод строки в текущую позицию курсора
		QTextCursor cursor = textCursor();
		if (cursor.position() < m_editableStart)
			return;
		cursor.insertText(QString("\n"));
		event->accept();
		return;
	}

	// Специальная обработка клавиш
	switch (event->key()) {
	case Qt::Key_Return:
	case Qt::Key_Enter: {
		if (getCurrentCommandLineText() == "clear") {
			clear();
			appendPrompt();
		}
		else {
			sendCurrentCommandLine();
		}
		break;
	}

	case Qt::Key_Backspace: {
		QTextCursor cursor = textCursor();
		if (cursor.hasSelection()) {
			int start = cursor.selectionStart();
			int end = cursor.selectionEnd();
			if (start >= m_editableStart && end >= m_editableStart)
			{
				QTextEdit::keyPressEvent(event);
			}
			else
			{
				cursor.setPosition(m_editableStart);
				cursor.setPosition(end, QTextCursor::KeepAnchor);
				setTextCursor(cursor);
			}
			break;
		}
		else if (cursor.position() <= m_editableStart) {
			break;
		}
		QTextEdit::keyPressEvent(event);
		break;
	}

	case Qt::Key_Delete: {
		QTextCursor cursor = textCursor();
		if (cursor.hasSelection()) {
			int start = cursor.selectionStart();
			int end = cursor.selectionEnd();
			if (start >= m_editableStart && end >= m_editableStart)
			{
				QTextEdit::keyPressEvent(event);
			}
			else
			{
				cursor.setPosition(m_editableStart);
				cursor.setPosition(end, QTextCursor::KeepAnchor);
				setTextCursor(cursor);
			}
		}
		else if (cursor.position() < m_editableStart) {
			break;
		}
		QTextEdit::keyPressEvent(event);
		break;
	}

	case Qt::Key_Left: {
		QTextCursor cur = textCursor();
		if (cur.position() <= m_editableStart && event->modifiers() != Qt::AltModifier) {
			break;
		}
		cur.movePosition(QTextCursor::Left);
		QTextEdit::keyPressEvent(event);
		break;
	}

	case Qt::Key_Home: {
		QTextCursor cur = textCursor();
		cur.setPosition(m_editableStart);
		setTextCursor(cur);
		break;
	}

	case Qt::Key_Up: {
		insertFromHistory(-1);
		break;
	}

	case Qt::Key_Down: {
		insertFromHistory(1);
		break;
	}

	case Qt::Key_Shift: {
		QTextEdit::keyPressEvent(event);
		break;
	}

	case Qt::Key_Control: {
		QTextEdit::keyPressEvent(event);
		break;
	}

	case Qt::Key_Alt: {
		QTextEdit::keyPressEvent(event);
		break;
	}

	default:
		// Ввод обычных символов: блокируем вставку в историю (курсор перемещается в конец editable-области)
		ensureCursorInEditableArea();
		setTextColor(Qt::white);
		QTextEdit::keyPressEvent(event);
		break;
	}
}

void Console::contextMenuEvent(QContextMenuEvent* event)
{
	QMenu* menu = new QMenu(this);

	// Стандартные действия с учетом editable области
	QAction* cutAction = menu->addAction(tr("Вырезать"));
	cutAction->setShortcut(QKeySequence::Cut);
	cutAction->setEnabled(textCursor().hasSelection() &&
		textCursor().selectionStart() >= m_editableStart);
	connect(cutAction, &QAction::triggered, [this]() {
		QTextCursor cursor = textCursor();
		if (cursor.hasSelection() &&
			cursor.selectionStart() >= m_editableStart &&
			cursor.selectionEnd() >= m_editableStart) {
			cut();
		}
		});

	QAction* copyAction = menu->addAction(tr("Копировать"));
	copyAction->setShortcut(QKeySequence::Copy);
	copyAction->setEnabled(textCursor().hasSelection());
	connect(copyAction, &QAction::triggered, [this]() {
		copy();
		});

	QAction* pasteAction = menu->addAction(tr("Вставить"));
	pasteAction->setShortcut(QKeySequence::Paste);
	pasteAction->setEnabled(canPaste());
	connect(pasteAction, &QAction::triggered, [this]() {
		ensureCursorInEditableArea();
		paste();
		});

	menu->addSeparator();

	// Выделить всё
	QAction* selectAllAction = menu->addAction(tr("Выделить всё"));
	selectAllAction->setShortcut(QKeySequence::SelectAll);
	connect(selectAllAction, &QAction::triggered, [this]() {
		QTextCursor cursor = textCursor();
		cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
		cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		setTextCursor(cursor);
		});

	// Выделить весь введённый текст
	QAction* selectAllInputAction = menu->addAction(tr("Выделить весь введённый текст"));
	selectAllInputAction->setShortcut(QKeySequence::SelectAll);
	connect(selectAllInputAction, &QAction::triggered, [this]() {
		QTextCursor cursor = textCursor();
		cursor.setPosition(m_editableStart);
		cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		setTextCursor(cursor);
		});

	menu->addSeparator();

	QAction* clearAction = menu->addAction(tr("Очистить консоль"));
	connect(clearAction, &QAction::triggered, [this]() {
		clear();
		appendPrompt();
		});

	QAction* copyOutputAction = menu->addAction(tr("Копировать последний вывод"));
	connect(copyOutputAction, &QAction::triggered, [this]() {
		QClipboard* clipboard = QApplication::clipboard();
		clipboard->setText(m_last_output);
		});

	QAction* debugMode = menu->addAction(tr("Режим отладки SBCL"));
	debugMode->setStatusTip(tr("Включение/отключение отладчика SBCL (требуется перезапуск ядра)"));
	debugMode->setCheckable(true);
	debugMode->setChecked(m_console_process->debugMode());

	connect(debugMode, &QAction::toggled, [this](bool checked) {
		m_console_process->setDebugMode(checked);
		appendOutput(tr("Перезапуск SBCL для применения режима отладки...\n"), false, true);

		// Останавливаем текущий процесс, если он запущен
		if (m_console_process) {
			m_console_process->stopLispProcess();
		}

		// Запускаем новый процесс
		if (!m_console_process->startLispProcess()) {
			appendOutput(tr("ОШИБКА: не удалось перезапустить SBCL\n"), true, true);
		}
		});

	QAction* formattedOutputAction = menu->addAction(tr("Выводить отформатированный ответ"));
	formattedOutputAction->setCheckable(true);
	formattedOutputAction->setChecked(m_console_process->formattedOutputOn());

	connect(formattedOutputAction, &QAction::toggled, [this](bool checked) {
		m_console_process->setFormattedOutputOn( checked );
		});

	menu->exec(event->globalPos());
	delete menu;
}

void Console::sendCurrentCommandLine()
{
	QString line = getCurrentCommandLineText();
	if (!line.isEmpty() && m_console_process) {
		m_console_process->sendCommand(line);
		appendOutput("\n");
	}
	else
	{
		appendPrompt(true);
	}
}

void Console::sendCode(const QString& code)
{
	clear();
	if (!code.isEmpty() && m_console_process) {
		m_console_process->sendCommand(code);
	}
}

void Console::sendFile(const QString& path)
{
	clear();
	if (path.isEmpty()) {
		appendOutput("ОШИБКА: путь к файлу не указан\n", true, true);
		return;
	}

	// Нормализуем путь: заменяем обратные слеши на прямые
	QString normalizedPath = path;
	normalizedPath.replace("\\", "/");

	// Формируем команду LOAD
	QString command = QString("(load \"%1\" :verbose t :print t)").arg(normalizedPath);

	// Отправляем команду
	if(m_console_process)
		m_console_process->sendCommand(command);
}

void Console::restartSbclProcess()
{
	if (m_console_process) {
		m_console_process->stopLispProcess();
		if (!m_console_process->startLispProcess()) {
			appendOutput(tr("ОШИБКА: не удалось перезапустить SBCL\n"), true, true);
		}
	}
}