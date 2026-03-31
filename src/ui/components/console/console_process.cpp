#include "console_process.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QApplication>
#include <QTimer>

ConsoleProcess::ConsoleProcess()
	: m_debug_mode(false)
	, m_formatted_output_on(true)
	, m_process(nullptr)
{
	startLispProcess();
}

ConsoleProcess::~ConsoleProcess()
{
	stopLispProcess();
}

bool ConsoleProcess::startLispProcess()
{
	// Если процесс уже запущен, не запускаем новый
	if (m_process && m_process->state() == QProcess::Running) {
		qDebug() << "Lisp процесс уже запущен";
		return true;
	}

	// Если процесс существует, но не запущен, останавливаем/освобождаем
	if (m_process) {
		stopLispProcess();
	}

	m_process = new QProcess(this);

	connect(m_process, &QProcess::started, this, &ConsoleProcess::onProcessStarted);
	connect(m_process, &QProcess::finished, this, &ConsoleProcess::onProcessFinished);
	connect(m_process, &QProcess::readyReadStandardOutput, this, &ConsoleProcess::onReadyReadStandardOutput);
	connect(m_process, &QProcess::readyReadStandardError, this, &ConsoleProcess::onReadyReadStandardError);
	connect(m_process, &QProcess::stateChanged, this, &ConsoleProcess::onProcessStateChanged);
	connect(m_process, &QProcess::errorOccurred, this, &ConsoleProcess::onProcessError);

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
		emit rawOutput(tr("ОШИБКА: исполняемый файл SBCL не найден. Поместите SBCL в папку 'sbcl' рядом с приложением или установите SBCL в систему.\n"), true, true);
		m_process->deleteLater();
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
		emit rawOutput("ОШИБКА: не удалось запустить SBCL в течение отведенного времени.\n", true, true);
		m_process->terminate();
		m_process->deleteLater();
		return false;
	}

	return true;
}

void ConsoleProcess::stopLispProcess()
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

	m_process->deleteLater();
}

void ConsoleProcess::sendCommand(const QString& command)
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

	if (bracketCount > 0) {
		emit rawOutput(tr("ОШИБКА: количество закрывающихся скобок меньше открывающихся!\n"), true, true);
	}
	else {
		// Отправляем команду в процесс
		QString cmd = command;
		if (!cmd.endsWith('\n')) {
			cmd += '\n';
		}
		m_process->write(cmd.toUtf8());

		// После записи, ConsoleProcess не модифицирует UI — Console ожидает сигнал readyForInput после получения вывода
	}
}

void ConsoleProcess::onProcessStarted()
{
	qDebug() << "Ядро Lisp запущено";
}

void ConsoleProcess::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
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
			emit rawOutput(tr("ОШИБКА: не удалось перезапустить ядро Lisp после %1 попыток!\n").arg(MAX_RETRIES), true, true);
		}
		else if (result) {
			emit rawOutput(tr("ПРЕДУПРЕЖДЕНИЕ: ядро Lisp было перезапущено\n"), true, true);
			emit readyForInput();
		}

		isRestarting = false;
		}
	);
}

void ConsoleProcess::onProcessError(QProcess::ProcessError error)
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
	emit rawOutput(tr("ОШИБКА: ") + errorMsg, true, true);
}

void ConsoleProcess::onReadyReadStandardOutput()
{
	if (!m_process) return;
	QByteArray data = m_process->readAllStandardOutput();
	QString output = QString::fromUtf8(data);
	if (output.trimmed().isEmpty())
		return;

	if (output == "* ")
		return;

	// Парсим и эмитим структурированное сообщение
	SBCLMessage formattedData = ConsoleParser::parse(output);

	// Эмитим как структурированное сообщение
	emit sbclMessage(formattedData);

	if (formattedData.type != SBCLMessageType::Success)
	{
		if (!formattedData.file.isEmpty())
			emit rawOutput(tr("Файл: %1\n").arg(formattedData.file), true);
		if (formattedData.type == SBCLMessageType::ReaderError)
			emit rawOutput(tr("Ошибка ридера!\n"), true);
		if (formattedData.type == SBCLMessageType::RuntimeError)
			emit rawOutput(tr("Ошибка при выполнении!\n"), true);
		if (formattedData.type == SBCLMessageType::Unknown)
			emit rawOutput(tr("Неизвестная ошибка!\n"), true);
		if (formattedData.line.has_value())
			emit rawOutput(tr("Строка: %1\n").arg(*formattedData.line), true);
		if (formattedData.column.has_value())
			emit rawOutput(tr("Позиция: %1\n").arg(*formattedData.column), true);

	}

	if (formattedData.type == SBCLMessageType::Success && !formattedData.message.isEmpty()) {
		emit rawOutput(formattedData.message, false, false);
	} else {
		// Если это ошибка/notice, передаём сообщение как заметку/ошибку
		emit rawOutput(formattedData.message.isEmpty() ? output : formattedData.message, true, false);
	}

	// Если это сообщение с позицией ошибки — эмитим старый сигнал errorOccurred (как раньше)
	if (formattedData.type != SBCLMessageType::Success &&
		formattedData.line.has_value() &&
		formattedData.column.has_value() &&
		!formattedData.file.isEmpty()) {

		emit errorOccurred(
			formattedData.file,
			formattedData.filePosition.value_or(-1),
			formattedData.message,
			formattedData.line.value_or(-1),
			formattedData.column.value_or(-1)
		);
	}

	// Сообщаем, что процесс готов для следующего ввода (Console вставит промпт)
	emit readyForInput();

	emit rawOutput("\n", false);
}

void ConsoleProcess::onReadyReadStandardError()
{
	if (!m_process) return;
	QByteArray data = m_process->readAllStandardError();
	QString error = QString::fromUtf8(data);
	emit rawOutput(error, true, true);
}

void ConsoleProcess::onProcessStateChanged(QProcess::ProcessState state)
{
	Q_UNUSED(state);
	emit processStateChanged(state);
}