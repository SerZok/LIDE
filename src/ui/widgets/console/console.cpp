#include "console.h"

#include <QScrollBar>
#include <QAction>
#include <QTextStream>
#include <QFileInfo>
#include <QThread>

Console::Console(QWidget* parent)
	: QTextEdit(parent)
	, m_process(nullptr)
	, m_historyIndex(-1)
	, m_prompt("CL-USER> ")
	, m_console_output_mark("SBCL: ")
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
	QTimer::singleShot(500, [this]() {

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
			
			counter++;
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
	);
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
		if (!formattedData.file.isEmpty())
			appendOutput(tr("Файл: %1\n").arg(formattedData.file), true);
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

	// Если это ошибка с позицией
	if (formattedData.type != SBCLMessageType::Success &&
		formattedData.line.has_value() &&
		formattedData.column.has_value() &&
		!formattedData.file.isEmpty()) {

		// Отправляем сигнал об ошибке
		emit errorOccurred(
			formattedData.file,
			formattedData.filePosition.value(),  // позиция в файле
			formattedData.message,
			formattedData.line.value_or(-1),
			formattedData.column.value_or(-1)
		);
	}

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