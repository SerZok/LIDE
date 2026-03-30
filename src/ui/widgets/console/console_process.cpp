#include "console.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>

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
	connect(m_process, &QProcess::readyReadStandardOutput, this, &Console::onReadyReadStandardOutput);
	connect(m_process, &QProcess::readyReadStandardError, this, &Console::onReadyReadStandardError);
	connect(m_process, &QProcess::stateChanged, this, &Console::onProcessStateChanged);
	connect(m_process, &QProcess::errorOccurred, this, &Console::onProcessError);

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
	if (!code.isEmpty()) {
		sendCommand(code);
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
	sendCommand(command);
}
