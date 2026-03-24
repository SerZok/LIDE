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

Console::Console(QWidget* parent)
	: QTextEdit(parent)
	, m_process(nullptr)
	, m_historyIndex(-1)
	, m_prompt("CL-USER> ")
	, m_waitingForInput(true)
	, m_editableStart(0)
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
	if (m_process && m_process->state() == QProcess::Running) {
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
		appendOutput("ERROR: SBCL executable not found. Place SBCL into 'sbcl' folder next to the application or install SBCL in the system PATH.\n", true, true);
		m_process->deleteLater();
		m_process = nullptr;
		return false;
	}

	m_process->setProcessChannelMode(QProcess::MergedChannels);

	QStringList args;
	args << "--noinform" << "--disable-debugger"; // оставляем интерактивность, без автоматического выхода

	m_process->start(exePath, args);

	if (!m_process->waitForStarted(3000)) {
		appendOutput("ERROR: Failed to start SBCL within timeout.\n", true, true);
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
		qDebug() << "Lisp process not running. Starting...";
		if (!startLispProcess()) {
			qDebug() << "Cannot send command: SBCL not available.";
			return;
		}
	}

	// Добавляем команду в историю
	if (!command.isEmpty()) {
		m_commandHistory.append(command);
		m_historyIndex = m_commandHistory.size();
	}

	if (bracketCount > 0) {
		appendOutput("Количество закрывающихся скобок меньше открывающихся!", true, true);
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
		else if (cursor.position() < m_editableStart + 1) {
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
	QMenu* menu = createStandardContextMenu();
	menu->addSeparator();

	QAction* clearAction = menu->addAction(tr("Очистить консоль"));
	connect(clearAction, &QAction::triggered, [this]() {
		clear();
		appendPrompt();
		});

	QAction* copyOutputAction = menu->addAction(tr("Копировать последний вывод"));
	connect(copyOutputAction, &QAction::triggered, [this]() {
		// TODO: Копирует только последний вывод
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
	bool result = false;
	int counter = 0;
	do
	{
		result = startLispProcess();
		++counter;
	} while (!result && counter < 5);
	if (!result && counter >= 5)
	{
		appendOutput("ОШИБКА: неудалось запустить ядро Lisp после его завершения!", true, true);
	}
	else
	{
		appendOutput("ПРЕДУПРЕЖДЕНИЕ: ядро Lisp было перезапущено!", true, true);
	}
}

void Console::onProcessError(QProcess::ProcessError error)
{
	QString errorMsg;
	switch (error) {
	case QProcess::FailedToStart:
		errorMsg = "Failed to start Lisp process. Check if SBCL is installed or bundled.";
		break;
	case QProcess::Crashed:
		errorMsg = "Lisp process crashed.";
		break;
	case QProcess::Timedout:
		errorMsg = "Operation timed out.";
		break;
	case QProcess::WriteError:
		errorMsg = "Write error.";
		break;
	case QProcess::ReadError:
		errorMsg = "Read error.";
		break;
	default:
		errorMsg = "Unknown error.";
	}
	appendOutput("ERROR: " + errorMsg, true, true);
}

void Console::onReadyReadStandardOutput()
{
	if (!m_process) return;
	QByteArray data = m_process->readAllStandardOutput();
	QString output = QString::fromUtf8(data);
	if (output != "* ") {
		SBCLMessage formattedData = ConsoleParser::parse(output);
		if (formattedData.type != SBCLMessageType::Success)
		{
			if(formattedData.type == SBCLMessageType::ReaderError)
				appendOutput("Ошибка ридера!\n", true);
			if (formattedData.type == SBCLMessageType::RuntimeError)
				appendOutput("Ошибка при выполнении!\n", true);
			if (formattedData.type == SBCLMessageType::Unknown)
				appendOutput("Неизвестная ошибка!\n", true);
			if(formattedData.line)
				appendOutput("Строка: " + formattedData.line.value() + '\n', true);
			if(formattedData.column)
				appendOutput("Позиция: " + formattedData.column.value() + '\n', true);
		}
		appendOutput(formattedData.message, false);
		m_lastInfo = formattedData;
		m_waitingForInput = true;
		appendPrompt();
	}
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

	// Проверки: не дублировать prompt, и вставлять его только после перевода строки
	QString doc = toPlainText();
	if (!doc.isEmpty()) {
		if (!doc.endsWith('\n')) {
			// Завершаем строку перед prompt, чтобы prompt был на новой строке
			moveCursor(QTextCursor::End);
			insertPlainText("\n");
		}
		else {
			// Если уже есть prompt в конце — не добавлять
			if (doc.endsWith(m_prompt)) {
				// Но нужно обновить m_editableStart
				QTextCursor cur = textCursor();
				moveCursor(QTextCursor::End);
				m_editableStart = textCursor().position();
				setTextCursor(cur);
				return;
			}
		}
	}

	moveCursor(QTextCursor::End);
	setTextColor(Qt::green);
	// Вставляем prompt и запоминаем позицию начала editable области
	insertPlainText(m_prompt);
	QTextCursor cur = textCursor();
	moveCursor(QTextCursor::End);
	m_editableStart = cur.position(); // после prompt
	// Отмечаем эту строку как редактируемую
	cur.block().setUserState(1);
	setTextCursor(cur);
	document()->clearUndoRedoStacks(QTextDocument::UndoStack);
	document()->clearUndoRedoStacks(QTextDocument::RedoStack);
	qDebug() << "m_editableStart:" << m_editableStart;
	setTextColor(Qt::white);
}

QString Console::getCurrentCommandLineText() const
{
	QTextCursor cursor = textCursor();
	// Выделяем от editableStart до конца строки
	cursor.setPosition(m_editableStart);
	cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
	QString text = cursor.selectedText();
	qDebug() << "text:" << text;
	return text.trimmed();
}

void Console::insertFromHistory(int direction)
{
	if (m_commandHistory.isEmpty()) return;

	if (m_historyIndex < 0) m_historyIndex = m_commandHistory.size();
	m_historyIndex += direction;
	m_historyIndex = qBound(0, m_historyIndex, m_commandHistory.size() - 1);

	QString command = m_commandHistory[m_historyIndex];

	// Заменяем текущую строку
	QTextCursor cursor = textCursor();
	cursor.setPosition(m_editableStart);
	cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
	cursor.removeSelectedText();
	cursor.insertText(command);
	// Помещаем курсор в конец вставленного текста
	cursor.setPosition(m_editableStart + command.length());
	setTextCursor(cursor);
}

void Console::ensureCursorInEditableArea()
{
	QTextCursor cur = textCursor();
	if (cur.position() < m_editableStart) {
		cur.setPosition(m_editableStart);
		setTextCursor(cur);
		cur.setPosition(m_editableStart + getCurrentCommandLineText().size());
		setTextCursor(cur);
	}
}

bool Console::cursorBeforeEditable() const
{
	QTextCursor cur = textCursor();
	return cur.position() < m_editableStart;
}