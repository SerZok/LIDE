#include "console.h"

#include <QScrollBar>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QThread>

Console::Console(QWidget* parent)
	: QTextEdit(parent)
	, m_process(nullptr)
	, m_historyIndex(-1)
	, m_prompt("CL-USER> ")
	, m_waitingForInput(true)
	, m_editableStart(0)
	, m_formatted_output_on(true)
	, m_debug_mode(false)
	, m_last_output("")
{
	setupConsole();
	startLispProcess();
	appendPrompt();
	m_waitingForInput = true;
}

Console::~Console()
{
	stopLispProcess();
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

bool Console::startLispProcess()
{
	// Если процесс уже запущен, не запускаем новый
	if (m_process && m_process->state() == QProcess::Running) {
		qDebug() << "Lisp процесс уже запущен";
		return true;
	}

	// Если процесс существует, но не запущен, удаляем его
	if (m_process) {
		stopLispProcess();
	}

	m_process = new QProcess(this);

	connect(m_process, &QProcess::started, this, &Console::onProcessStarted);
	connect(m_process, &QProcess::finished, this, &Console::onProcessFinished);
	connect(m_process, &QProcess::errorOccurred, this, &Console::onProcessError);
	connect(m_process, &QProcess::readyReadStandardOutput, this, &Console::onReadyReadStandardOutput);
	connect(m_process, &QProcess::readyReadStandardError, this, &Console::onReadyReadStandardError);
	connect(m_process, &QProcess::stateChanged, this, &Console::onProcessStateChanged);

	// Поиск исполняемого SBCL:
	QString exePath;
#ifdef Q_OS_WIN
	QString bundled = QCoreApplication::applicationDirPath() + QDir::separator() + "sbcl" + QDir::separator() + "sbcl.exe";
#else
	QString bundled = QCoreApplication::applicationDirPath() + QDir::separator() + "sbcl" + QDir::separator() + "sbcl";
#endif

	if (QFile::exists(bundled) && QFileInfo(bundled).isExecutable()) {
		exePath = bundled;
		m_process->setWorkingDirectory(QFileInfo(bundled).absolutePath());
	}
	else {
		QString systemExe = QStandardPaths::findExecutable("sbcl");
		if (!systemExe.isEmpty()) {
			exePath = systemExe;
		}
	}

	if (exePath.isEmpty()) {
		appendOutput(tr("ОШИБКА: исполняемый файл SBCL не найден. Поместите SBCL в папку 'sbcl' рядом с приложением или установите SBCL в систему.\n"), true, true);
		m_process->deleteLater();
		m_process = nullptr;
		return false;
	}

	m_process->setProcessChannelMode(QProcess::MergedChannels);

	QStringList args;
	args << "--noinform";
	if (!m_debug_mode)
	{
		args << "--disable-debugger";
	}

	m_process->start(exePath, args);

	if (!m_process->waitForStarted(3000)) {
		appendOutput("ОШИБКА: не удалось запустить SBCL в течение отведенного времени.\n", true, true);
		m_process->terminate();
		m_process->deleteLater();
		m_process = nullptr;
		return false;
	}

	return true;
}

void Console::stopLispProcess()
{
	if (!m_process) return;

	// Отключаем сигналы, чтобы избежать лишних вызовов
	m_process->disconnect();

	if (m_process->state() == QProcess::Running) {
		qDebug() << "Завершение Lisp процесса...";
		m_process->terminate();

		if (!m_process->waitForFinished(1000)) {
			qDebug() << "Процесс не завершился, убиваем...";
			m_process->kill();
			m_process->waitForFinished(1000);
		}
	}

	delete m_process;
	m_process = nullptr;
}

void Console::sendCommand(const QString& command)
{
	int bracketCount = 0;
	for (auto curChar : command) {
		if (curChar == '(')
			++bracketCount;
		if (curChar == ')')
			--bracketCount;
	}

	if (!m_process || m_process->state() != QProcess::Running) {
		qDebug() << "Lisp процесс не запущен. Запускаем...";
		if (!startLispProcess()) {
			qDebug() << "Не могу отправить команду: SBCL недоступен.";
			return;
		}
	}

	// Добавляем команду в историю
	if (!command.isEmpty()) {
		m_commandHistory.append(command);
		m_historyIndex = m_commandHistory.size();
	}

	if (bracketCount > 0) {
		appendOutput(tr("ОШИБКА: количество закрывающихся скобок меньше открывающихся!\n"), true, true);
	}
	else {
		// Отправляем команду в процесс
		QString cmd = command;
		if (!cmd.endsWith('\n')) {
			cmd += '\n';
		}
		m_process->write(cmd.toUtf8());

		// Блокируем ввод пока не получим результат
		m_waitingForInput = false;
	}
}

void Console::sendCurrentCommandLine()
{
	QString line = getCurrentCommandLineText();
	if (!line.isEmpty()) {
		sendCommand(line);
	}
}

void Console::sendCode(const QString& code)
{
	clear();
	if (!code.isEmpty()) {
		sendCommand(code);
	}
}

void Console::sendSelectedText()
{
	QString selected = textCursor().selectedText();
	if (!selected.isEmpty()) {
		sendCommand(selected);
	}
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
			if (m_process && m_process->state() == QProcess::Running) {
				m_process->kill(); // или лучше interrupt (см. ниже)
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

	// Обрабатываем сочетания Shift+Enter — ставим перевод строки
	if (event->key() == Qt::Key_Enter && event->modifiers() == Qt::ShiftModifier)
	{
		appendOutput("\n");
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
			appendOutput("\n");
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
		}
		else if (cursor.position() <= m_editableStart) {
			return;
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
			return;
		}
		QTextEdit::keyPressEvent(event);
		break;
	}

	case Qt::Key_Left: {
		QTextCursor cur = textCursor();
		QTextCursor moved = cur;
		moved.movePosition(QTextCursor::Left);
		if (moved.position() < m_editableStart) {
			// Переместить на начало editable-области
			cur.setPosition(m_editableStart);
			setTextCursor(cur);
			return;
		}
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
	debugMode->setChecked(m_debug_mode);

	connect(debugMode, &QAction::toggled, [this](bool checked) {
		m_debug_mode = checked;
		appendOutput(tr("Перезапуск SBCL для применения режима отладки...\n"), false, true);

		// Останавливаем текущий процесс, если он запущен
		if (m_process && m_process->state() == QProcess::Running) {
			stopLispProcess();
		}

		// Запускаем новый процесс
		if (!startLispProcess()) {
			appendOutput(tr("ОШИБКА: не удалось перезапустить SBCL\n"), true, true);
		}
		});

	QAction* formattedOutputAction = menu->addAction(tr("Выводить отформатированный ответ"));
	formattedOutputAction->setCheckable(true);
	formattedOutputAction->setChecked(m_formatted_output_on);

	connect(formattedOutputAction, &QAction::toggled, [this](bool checked) {
		m_formatted_output_on = checked;
		});

	menu->exec(event->globalPos());
	delete menu;
}

void Console::onProcessStarted()
{
	qDebug() << "Ядро Lisp запущено";
}

void Console::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	Q_UNUSED(exitCode);
	Q_UNUSED(exitStatus);

	// Защита от множественных вызовов
	static bool isRestarting = false;
	if (isRestarting) {
		qDebug() << "Уже выполняется перезапуск, пропускаем...";
		return;
	}

	isRestarting = true;

	// Небольшая задержка перед перезапуском
	QThread::msleep(500);

	bool result = false;
	int counter = 0;
	const int MAX_RETRIES = 5;

	while (!result && counter < MAX_RETRIES) {
		qDebug() << "Попытка перезапуска Lisp ядра..." << counter + 1;

		// Проверяем, не запущен ли уже процесс
		if (m_process && m_process->state() == QProcess::Running) {
			qDebug() << "Процесс уже запущен, перезапуск не требуется";
			result = true;
			break;
		}

		result = startLispProcess();

		if (!result) {
			counter++;
			if (counter < MAX_RETRIES) {
				// Увеличиваем задержку с каждой попыткой
				QThread::msleep(500 * counter);
			}
		}
	}

	if (!result && counter >= MAX_RETRIES) {
		appendOutput(tr("ОШИБКА: не удалось перезапустить ядро Lisp после %1 попыток!\n").arg(MAX_RETRIES), true, true);
	}
	else if (result) {
		appendOutput(tr("ПРЕДУПРЕЖДЕНИЕ: ядро Lisp было перезапущено\n"), true, true);
		appendPrompt();
	}

	isRestarting = false;
}

void Console::onProcessError(QProcess::ProcessError error)
{
	QString errorMsg;
	switch (error) {
	case QProcess::FailedToStart:
		errorMsg = tr("Не удалось запустить процесс Lisp. Проверьте, установлен ли SBCL или находится ли он в папке приложения.");
		break;
	case QProcess::Crashed:
		errorMsg = tr("Процесс Lisp аварийно завершился.");
		break;
	case QProcess::Timedout:
		errorMsg = tr("Превышено время ожидания операции.");
		break;
	case QProcess::WriteError:
		errorMsg = tr("Ошибка записи.");
		break;
	case QProcess::ReadError:
		errorMsg = tr("Ошибка чтения.");
		break;
	default:
		errorMsg = tr("Неизвестная ошибка.");
	}
	appendOutput(tr("ОШИБКА: ") + errorMsg, true, true);
}

void Console::onReadyReadStandardOutput()
{
	if (!m_process) return;
	QByteArray data = m_process->readAllStandardOutput();
	QString output = QString::fromUtf8(data);
	if (output == "* ")
		return;

	SBCLMessage formattedData = ConsoleParser::parse(output);
	if (formattedData.type != SBCLMessageType::Success)
	{
		if (formattedData.type == SBCLMessageType::ReaderError)
			appendOutput(tr("Ошибка ридера!\n"), true);
		if (formattedData.type == SBCLMessageType::RuntimeError)
			appendOutput(tr("Ошибка при выполнении!\n"), true);
		if (formattedData.type == SBCLMessageType::Unknown)
			appendOutput(tr("Неизвестная ошибка!\n"), true);
		if (formattedData.line.has_value())
			appendOutput(tr("Строка: %1\n").arg(*formattedData.line), true);
		if (formattedData.column.has_value())
			appendOutput(tr("Позиция: %1\n").arg(*formattedData.column), true);

	}

	m_lastInfo = formattedData;

	if ( m_formatted_output_on ) 
	{
		m_last_output = formattedData.message;
		appendOutput(formattedData.message, false);
	}
	else
	{
		m_last_output = output;
		appendOutput(output, false);
	}
	m_waitingForInput = true;
	appendPrompt();
}

void Console::onReadyReadStandardError()
{
	if (!m_process) return;
	QByteArray data = m_process->readAllStandardError();
	QString error = QString::fromUtf8(data);
	appendOutput(error, true, true);
}

void Console::onProcessStateChanged(QProcess::ProcessState state)
{
	Q_UNUSED(state);
	emit processStateChanged(state);
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

void Console::appendPrompt()
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
	if (lastLine == m_prompt) {
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

	qDebug() << "m_editableStart:" << m_editableStart;
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

bool Console::cursorBeforeEditable() const
{
	QTextCursor cur = textCursor();
	return cur.position() < m_editableStart;
}

void Console::clear()
{
	QTextEdit::clear();
	m_editableStart = 0;
	m_waitingForInput = true;
}