#include "console.h"

#include <QMenu>
#include <QApplication>
#include <QClipboard>

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
				m_process->write("(sb-ext:quit)\n");
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
