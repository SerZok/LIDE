#include "sbcl_sandbox.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>

#include <algorithm>
#include <QStringList>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <aclapi.h>
#include <userenv.h>
#endif

#ifdef Q_OS_WIN
namespace
{
constexpr qint64 kMaxInstallationBytes = 128 * 1024 * 1024;
constexpr DWORD kMaxOutputBytes = 64 * 1024;
constexpr DWORD kTimeoutMs = 15000;
constexpr SIZE_T kMemoryLimitBytes = 512 * 1024 * 1024;

class ScopedHandle
{
public:
    explicit ScopedHandle(HANDLE handle = nullptr) : m_handle(handle) {}
    ~ScopedHandle()
    {
        if (m_handle && m_handle != INVALID_HANDLE_VALUE)
            CloseHandle(m_handle);
    }

    ScopedHandle(const ScopedHandle &) = delete;
    ScopedHandle &operator=(const ScopedHandle &) = delete;

    HANDLE get() const { return m_handle; }
    HANDLE *address() { return &m_handle; }
    HANDLE release()
    {
        HANDLE handle = m_handle;
        m_handle = nullptr;
        return handle;
    }
    explicit operator bool() const { return m_handle && m_handle != INVALID_HANDLE_VALUE; }
    void reset()
    {
        if (m_handle && m_handle != INVALID_HANDLE_VALUE)
            CloseHandle(m_handle);
        m_handle = nullptr;
    }

private:
    HANDLE m_handle;
};

struct AppContainerProfile
{
    PSID sid = nullptr;
    QString name;

    ~AppContainerProfile()
    {
        if (!name.isEmpty())
            DeleteAppContainerProfile(reinterpret_cast<LPCWSTR>(name.utf16()));
        if (sid)
            FreeSid(sid);
    }
};

QString windowsError(DWORD error)
{
    return QStringLiteral("Windows error %1").arg(error);
}

bool createProfile(AppContainerProfile &profile, QString &error)
{
    profile.name = QStringLiteral("LIDE.Benchmark.%1")
                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const HRESULT result = CreateAppContainerProfile(
        reinterpret_cast<LPCWSTR>(profile.name.utf16()), L"LIDE Benchmark",
        L"Isolated SBCL benchmark process", nullptr, 0, &profile.sid);
    if (SUCCEEDED(result))
        return true;

    error = QStringLiteral("Could not create Windows AppContainer profile (%1).")
                .arg(QString::number(static_cast<quint32>(result), 16));
    return false;
}

bool grantSandboxDirectoryAccess(const QString &directory, PSID sid, QString &error)
{
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    PACL oldDacl = nullptr;
    const DWORD getResult = GetNamedSecurityInfoW(
        const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(directory.utf16())),
        SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &oldDacl,
        nullptr, &descriptor);
    if (getResult != ERROR_SUCCESS)
    {
        error = QStringLiteral("Could not read sandbox directory permissions: %1")
                    .arg(windowsError(getResult));
        return false;
    }

    EXPLICIT_ACCESSW access{};
    access.grfAccessPermissions = FILE_GENERIC_READ | FILE_GENERIC_EXECUTE | SYNCHRONIZE;
    access.grfAccessMode = GRANT_ACCESS;
    access.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    access.Trustee.TrusteeType = TRUSTEE_IS_USER;
    access.Trustee.ptstrName = static_cast<LPWSTR>(sid);

    PACL newDacl = nullptr;
    const DWORD aclResult = SetEntriesInAclW(1, &access, oldDacl, &newDacl);
    if (aclResult != ERROR_SUCCESS)
    {
        LocalFree(descriptor);
        error = QStringLiteral("Could not set sandbox directory permissions: %1")
                    .arg(windowsError(aclResult));
        return false;
    }

    const DWORD setResult = SetNamedSecurityInfoW(
        const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(directory.utf16())),
        SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, newDacl, nullptr);
    LocalFree(newDacl);
    LocalFree(descriptor);
    if (setResult != ERROR_SUCCESS)
    {
        error = QStringLiteral("Could not apply sandbox directory permissions: %1")
                    .arg(windowsError(setResult));
        return false;
    }
    return true;
}

bool copyInstallation(const QString &sourceDirectory, const QString &destinationDirectory,
                      QString &error)
{
    QDir source(sourceDirectory);
    qint64 copiedBytes = 0;
    int copiedEntries = 0;
    QVector<QPair<QString, QString>> pending;
    pending.append({source.absolutePath(), destinationDirectory});

    while (!pending.isEmpty())
    {
        const auto current = pending.takeLast();
        if (!QDir().mkpath(current.second))
        {
            error = QStringLiteral("Could not create isolated SBCL directory.");
            return false;
        }

        QDir currentSource(current.first);
        const QFileInfoList children = currentSource.entryInfoList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
        for (const QFileInfo &entry : children)
        {
            if (++copiedEntries > 10000)
            {
                error = QStringLiteral("SBCL installation contains too many files to copy.");
                return false;
            }
            const QString destination = QDir(current.second).filePath(entry.fileName());
            if (entry.isDir())
            {
                pending.append({entry.absoluteFilePath(), destination});
                continue;
            }

            copiedBytes += entry.size();
            if (copiedBytes > kMaxInstallationBytes)
            {
                error = QStringLiteral("SBCL installation is too large for the isolated runner.");
                return false;
            }
            if (!QFile::copy(entry.absoluteFilePath(), destination))
            {
                error = QStringLiteral("Could not copy SBCL runtime file: %1").arg(entry.fileName());
                return false;
            }
        }
    }
    return true;
}

QString escapeLispString(QString value)
{
    value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    value.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    value.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    value.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    return value;
}

QString quoteWindowsArgument(const QString &argument)
{
    QString quoted = QStringLiteral("\"");
    int backslashes = 0;
    for (const QChar character : argument)
    {
        if (character == QLatin1Char('\\'))
        {
            ++backslashes;
        }
        else if (character == QLatin1Char('"'))
        {
            quoted += QString(backslashes * 2 + 1, QLatin1Char('\\'));
            quoted += character;
            backslashes = 0;
        }
        else
        {
            quoted += QString(backslashes, QLatin1Char('\\'));
            backslashes = 0;
            quoted += character;
        }
    }
    quoted += QString(backslashes * 2, QLatin1Char('\\'));
    quoted += QLatin1Char('"');
    return quoted;
}

std::vector<wchar_t> toWideBuffer(const QString &value)
{
    const auto *characters = reinterpret_cast<const wchar_t *>(value.utf16());
    std::vector<wchar_t> buffer(characters, characters + value.size());
    buffer.push_back(L'\0');
    return buffer;
}

std::vector<wchar_t> makeEnvironmentBlock(const QString &sandboxPath,
                                          const QString &sbclDirectory)
{
    wchar_t systemDirectory[MAX_PATH]{};
    const UINT systemDirectoryLength = GetWindowsDirectoryW(systemDirectory, MAX_PATH);
    const QString systemRoot = systemDirectoryLength > 0 && systemDirectoryLength < MAX_PATH
                                   ? QString::fromWCharArray(systemDirectory, systemDirectoryLength)
                                   : QString();
    const QString nativeSandboxPath = QDir::toNativeSeparators(sandboxPath);
    QStringList variables{
        QStringLiteral("APPDATA=%1").arg(sandboxPath),
        QStringLiteral("HOME=%1").arg(sandboxPath),
        QStringLiteral("LOCALAPPDATA=%1").arg(sandboxPath),
        QStringLiteral("PATH=%1;%2\\System32").arg(sbclDirectory, systemRoot),
        QStringLiteral("SBCL_HOME=%1").arg(sbclDirectory),
        QStringLiteral("SystemRoot=%1").arg(systemRoot),
        QStringLiteral("TEMP=%1").arg(sandboxPath),
        QStringLiteral("TMP=%1").arg(sandboxPath),
        QStringLiteral("USERPROFILE=%1").arg(sandboxPath),
        QStringLiteral("WINDIR=%1").arg(systemRoot)};
    std::sort(variables.begin(), variables.end(), [](const QString &left, const QString &right) {
        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    });

    std::vector<wchar_t> environment;
    const int driveSeparator = nativeSandboxPath.indexOf(QLatin1Char(':'));
    if (driveSeparator == 1)
    {
        const QString driveDirectory =
            QStringLiteral("=%1:=%2").arg(nativeSandboxPath.left(1), nativeSandboxPath);
        for (const QChar character : driveDirectory)
            environment.push_back(static_cast<wchar_t>(character.unicode()));
        environment.push_back(L'\0');
    }
    for (const QString &variable : variables)
    {
        for (const QChar character : variable)
            environment.push_back(static_cast<wchar_t>(character.unicode()));
        environment.push_back(L'\0');
    }
    environment.push_back(L'\0');
    return environment;
}

QJsonObject failureResult(const QString &status, const QString &message, qint64 elapsedMs = 0)
{
    return QJsonObject{{"status", status},
                       {"message", message},
                       {"elapsedMs", elapsedMs}};
}

QJsonObject launchSandboxed(const QString &sbclPath, const QString &sandboxPath,
                            const QString &scriptPath, const QString &sbclDirectory,
                            AppContainerProfile &profile)
{
    ScopedHandle stdoutRead;
    ScopedHandle stdoutWrite;
    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;
    if (!CreatePipe(stdoutRead.address(), stdoutWrite.address(), &securityAttributes, 0))
        return failureResult(QStringLiteral("sandbox-error"),
                             QStringLiteral("Could not create output pipe: %1")
                                 .arg(windowsError(GetLastError())));
    if (!SetHandleInformation(stdoutRead.get(), HANDLE_FLAG_INHERIT, 0))
        return failureResult(QStringLiteral("sandbox-error"),
                             QStringLiteral("Could not secure output pipe: %1")
                                 .arg(windowsError(GetLastError())));

    ScopedHandle stdinHandle(CreateFileW(
        L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &securityAttributes,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!stdinHandle)
        return failureResult(QStringLiteral("sandbox-error"),
                             QStringLiteral("Could not open null input: %1")
                                 .arg(windowsError(GetLastError())));

    SIZE_T attributeListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 2, 0, &attributeListSize);
    QByteArray attributeStorage(static_cast<qsizetype>(attributeListSize), Qt::Uninitialized);
    auto *attributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        attributeStorage.data());
    if (!InitializeProcThreadAttributeList(attributeList, 2, 0, &attributeListSize))
        return failureResult(QStringLiteral("sandbox-error"),
                             QStringLiteral("Could not initialize process security attributes: %1")
                                 .arg(windowsError(GetLastError())));

    SECURITY_CAPABILITIES capabilities{};
    capabilities.AppContainerSid = profile.sid;
    capabilities.CapabilityCount = 0;
    capabilities.Capabilities = nullptr;
    capabilities.Reserved = 0;
    if (!UpdateProcThreadAttribute(attributeList, 0, PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
                                   &capabilities, sizeof(capabilities), nullptr, nullptr))
    {
        const QString updateError = windowsError(GetLastError());
        DeleteProcThreadAttributeList(attributeList);
        return failureResult(QStringLiteral("sandbox-error"),
                             QStringLiteral("Could not configure AppContainer: %1").arg(updateError));
    }
    HANDLE inheritedHandles[]{stdinHandle.get(), stdoutWrite.get()};
    if (!UpdateProcThreadAttribute(attributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                   inheritedHandles, sizeof(inheritedHandles), nullptr, nullptr))
    {
        const QString updateError = windowsError(GetLastError());
        DeleteProcThreadAttributeList(attributeList);
        return failureResult(QStringLiteral("sandbox-error"),
                             QStringLiteral("Could not restrict inherited handles: %1")
                                 .arg(updateError));
    }

    STARTUPINFOEXW startupInfo{};
    startupInfo.StartupInfo.cb = sizeof(startupInfo);
    startupInfo.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.StartupInfo.hStdInput = stdinHandle.get();
    startupInfo.StartupInfo.hStdOutput = stdoutWrite.get();
    startupInfo.StartupInfo.hStdError = stdoutWrite.get();
    startupInfo.lpAttributeList = attributeList;

    const QString commandLine =
        quoteWindowsArgument(sbclPath) +
        QStringLiteral(" --noinform --no-sysinit --no-userinit --disable-debugger --script ") +
        quoteWindowsArgument(scriptPath);
    std::vector<wchar_t> mutableCommandLine = toWideBuffer(commandLine);
    std::vector<wchar_t> mutableEnvironment =
        makeEnvironmentBlock(sandboxPath, sbclDirectory);

    PROCESS_INFORMATION processInfo{};
    const BOOL created = CreateProcessW(
        reinterpret_cast<LPCWSTR>(sbclPath.utf16()), mutableCommandLine.data(),
        nullptr, nullptr, TRUE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
        mutableEnvironment.data(), reinterpret_cast<LPCWSTR>(sandboxPath.utf16()),
        &startupInfo.StartupInfo, &processInfo);
    const DWORD createError = created ? ERROR_SUCCESS : GetLastError();
    DeleteProcThreadAttributeList(attributeList);
    stdoutWrite.reset();
    stdinHandle.reset();
    if (!created)
        return failureResult(QStringLiteral("sandbox-error"),
                             QStringLiteral("Could not start isolated SBCL: %1")
                                 .arg(windowsError(createError)));

    ScopedHandle process(processInfo.hProcess);
    ScopedHandle primaryThread(processInfo.hThread);
    ScopedHandle job(CreateJobObjectW(nullptr, nullptr));
    if (!job)
    {
        TerminateProcess(process.get(), 1);
        WaitForSingleObject(process.get(), INFINITE);
        return failureResult(QStringLiteral("sandbox-error"),
                             QStringLiteral("Could not create SBCL job: %1")
                                 .arg(windowsError(GetLastError())));
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
        JOB_OBJECT_LIMIT_ACTIVE_PROCESS |
        JOB_OBJECT_LIMIT_PROCESS_MEMORY |
        JOB_OBJECT_LIMIT_JOB_TIME;
    limits.BasicLimitInformation.ActiveProcessLimit = 1;
    limits.BasicLimitInformation.PerJobUserTimeLimit.QuadPart =
        static_cast<LONGLONG>(kTimeoutMs) * 10000;
    limits.ProcessMemoryLimit = kMemoryLimitBytes;
    limits.JobMemoryLimit = kMemoryLimitBytes;
    if (!SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                                 &limits, sizeof(limits)) ||
        !AssignProcessToJobObject(job.get(), process.get()))
    {
        const QString jobError = windowsError(GetLastError());
        TerminateProcess(process.get(), 1);
        WaitForSingleObject(process.get(), INFINITE);
        return failureResult(QStringLiteral("sandbox-error"),
                             QStringLiteral("Could not enforce SBCL process limits: %1").arg(jobError));
    }

    if (ResumeThread(primaryThread.get()) == static_cast<DWORD>(-1))
    {
        const QString resumeError = windowsError(GetLastError());
        TerminateJobObject(job.get(), 1);
        WaitForSingleObject(process.get(), INFINITE);
        return failureResult(QStringLiteral("sandbox-error"),
                             QStringLiteral("Could not start isolated SBCL: %1").arg(resumeError));
    }

    QElapsedTimer timer;
    timer.start();
    QByteArray output;
    bool timedOut = false;
    bool outputExceeded = false;
    DWORD exitCode = STILL_ACTIVE;
    while (true)
    {
        DWORD available = 0;
        if (PeekNamedPipe(stdoutRead.get(), nullptr, 0, nullptr, &available, nullptr) &&
            available > 0)
        {
            char buffer[4096];
            DWORD bytesRead = 0;
            const DWORD bytesToRead = qMin<DWORD>(available, sizeof(buffer));
            if (ReadFile(stdoutRead.get(), buffer, bytesToRead, &bytesRead, nullptr) && bytesRead > 0)
            {
                if (output.size() + static_cast<qsizetype>(bytesRead) > kMaxOutputBytes)
                    outputExceeded = true;
                else
                    output.append(buffer, static_cast<qsizetype>(bytesRead));
            }
            if (outputExceeded)
                break;
        }

        const DWORD waitResult = WaitForSingleObject(process.get(), 20);
        if (waitResult == WAIT_OBJECT_0)
        {
            GetExitCodeProcess(process.get(), &exitCode);
            while (PeekNamedPipe(stdoutRead.get(), nullptr, 0, nullptr, &available, nullptr) &&
                   available > 0 && output.size() <= kMaxOutputBytes)
            {
                char buffer[4096];
                DWORD bytesRead = 0;
                if (!ReadFile(stdoutRead.get(), buffer,
                              qMin<DWORD>(available, sizeof(buffer)), &bytesRead, nullptr) ||
                    bytesRead == 0)
                    break;
                if (output.size() + static_cast<qsizetype>(bytesRead) > kMaxOutputBytes)
                {
                    outputExceeded = true;
                    break;
                }
                output.append(buffer, static_cast<qsizetype>(bytesRead));
            }
            break;
        }
        if (waitResult == WAIT_FAILED)
        {
            const QString waitError = windowsError(GetLastError());
            TerminateJobObject(job.get(), 1);
            WaitForSingleObject(process.get(), INFINITE);
            return failureResult(QStringLiteral("sandbox-error"),
                                 QStringLiteral("Could not monitor SBCL process: %1").arg(waitError),
                                 timer.elapsed());
        }
        if (timer.elapsed() >= kTimeoutMs)
        {
            timedOut = true;
            break;
        }
        QThread::msleep(5);
    }

    if (timedOut || outputExceeded)
    {
        TerminateJobObject(job.get(), 1);
        WaitForSingleObject(process.get(), INFINITE);
    }
    else
        WaitForSingleObject(process.get(), INFINITE);

    if (timedOut)
        return failureResult(QStringLiteral("timeout"),
                             QStringLiteral("SBCL exceeded the 15 second time limit."),
                             timer.elapsed());
    if (outputExceeded)
        return failureResult(QStringLiteral("output-limit"),
                             QStringLiteral("SBCL output exceeded the 64 KiB limit."),
                             timer.elapsed());

    const QString outputText = QString::fromUtf8(output);
    const QRegularExpression summaryExpression(QStringLiteral("LIDE_RESULT:(\\d+)/(\\d+)"));
    const QRegularExpressionMatch summaryMatch = summaryExpression.match(outputText);
    if (!summaryMatch.hasMatch())
        return failureResult(QStringLiteral("failed"),
                             QStringLiteral("SBCL did not complete the test harness (exit code %1).")
                                 .arg(exitCode),
                             timer.elapsed());

    const int passed = summaryMatch.captured(1).toInt();
    const int total = summaryMatch.captured(2).toInt();
    return QJsonObject{
        {"status", exitCode == 0 && passed == total ? "passed" : "failed"},
        {"passedTests", passed},
        {"totalTests", total},
        {"exitCode", static_cast<int>(exitCode)},
        {"output", outputText},
        {"elapsedMs", timer.elapsed()}};
}

QString makeHarness(const QString &sourcePath, const QString &functionName,
                    const QVector<SbclSandbox::TestCase> &tests)
{
    QString harness = QStringLiteral(
        "(cl:setf cl:*read-eval* cl:nil)\n"
        "(cl:let ((passed 0) (total %1))\n"
        "  (cl:labels ((check (name expected thunk)\n"
        "               (cl:handler-case\n"
        "                   (cl:let ((actual (cl:funcall thunk)))\n"
        "                     (cl:if (cl:equal expected actual)\n"
        "                            (cl:progn (cl:incf passed) "
        "(cl:format cl:*standard-output* \"LIDE_TEST_PASS:~A~%\" name))\n"
        "                            (cl:format cl:*standard-output* "
        "\"LIDE_TEST_FAIL:~A expected=~S actual=~S~%\" name expected actual)))\n"
        "                 (cl:error (condition)\n"
        "                   (cl:format cl:*standard-output* \"LIDE_TEST_ERROR:~A ~A~%\" "
        "name condition)))))\n"
        "    (cl:handler-case\n"
        "        (cl:let ((cl:*package* (cl:find-package \"CL-USER\")))\n"
        "          (cl:load \"%2\" :verbose cl:nil :print cl:nil)\n")
        .arg(tests.size())
        .arg(escapeLispString(sourcePath));

    const QString functionSymbol = escapeLispString(functionName.toUpper());
    for (const auto &test : tests)
    {
        const QString expression =
            QStringLiteral("(cl:funcall (cl:symbol-function (cl:find-symbol \"%1\" \"CL-USER\")) %2)")
                .arg(functionSymbol, test.arguments);
        harness += QStringLiteral("          (check \"%1\" %2 (cl:lambda () %3))\n")
                       .arg(escapeLispString(test.name), test.expected, expression);
    }

    harness += QStringLiteral(
        "          (cl:format cl:*standard-output* \"LIDE_RESULT:~D/~D~%\" passed total)\n"
        "          (sb-ext:exit :code (cl:if (cl:= passed total) 0 1)))\n"
        "      (cl:error (condition)\n"
        "        (cl:format cl:*standard-output* \"LIDE_HARNESS_ERROR:~A~%\" condition)\n"
        "        (cl:format cl:*standard-output* \"LIDE_RESULT:~D/~D~%\" passed total)\n"
        "        (sb-ext:exit :code 2)))))\n");
    return harness;
}
}
#endif

QJsonObject SbclSandbox::run(const QString &sbclExecutable, const QString &modelResponse,
                             const QString &functionName, const QVector<TestCase> &tests)
{
#ifndef Q_OS_WIN
    Q_UNUSED(sbclExecutable);
    Q_UNUSED(modelResponse);
    Q_UNUSED(functionName);
    Q_UNUSED(tests);
    return {{"status", "unsupported"},
            {"message", "The isolated SBCL runner is only supported on Windows."}};
#else
    const QFileInfo executableInfo(sbclExecutable);
    if (!executableInfo.isFile())
        return failureResult(QStringLiteral("unavailable"),
                             QStringLiteral("The configured SBCL executable was not found."));
    if (functionName.isEmpty() || tests.isEmpty())
        return failureResult(QStringLiteral("unsupported"),
                             QStringLiteral("No executable tests are defined for this task."));

    static const QRegularExpression codeBlock(
        QStringLiteral(
            "(?:^|\\r?\\n)[ \\t]*```[ \\t]*(?:(?:common-lisp|lisp|cl)[ \\t]*)?"
            "(?:\\r?\\n([\\s\\S]*?)(?:\\r?\\n)?[ \\t]*```[ \\t]*(?=\\r?$)|"
            "[ \\t]*```[ \\t]*(?=\\r?$))"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator sourceMatches = codeBlock.globalMatch(modelResponse);
    QString sourceCode;
    while (sourceMatches.hasNext() && sourceCode.isEmpty())
        sourceCode = sourceMatches.next().captured(1).trimmed();
    if (sourceCode.isEmpty())
        return failureResult(QStringLiteral("no-code"),
                             QStringLiteral("No Lisp code block was found in the model response."));

    QTemporaryDir temporaryDirectory(QDir::tempPath() + QStringLiteral("/lide-sbcl-XXXXXX"));
    if (!temporaryDirectory.isValid())
        return failureResult(QStringLiteral("sandbox-error"),
                             QStringLiteral("Could not create the isolated SBCL directory."));

    AppContainerProfile profile;
    QString error;
    if (!createProfile(profile, error))
        return failureResult(QStringLiteral("sandbox-error"), error);
    if (!grantSandboxDirectoryAccess(temporaryDirectory.path(), profile.sid, error))
        return failureResult(QStringLiteral("sandbox-error"), error);

    const QString sbclDirectory =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("runtime"));
    if (!copyInstallation(executableInfo.absolutePath(), sbclDirectory, error))
        return failureResult(QStringLiteral("sandbox-error"), error);

    const QString candidatePath = QDir(temporaryDirectory.path()).filePath("candidate.lisp");
    QFile candidate(candidatePath);
    if (!candidate.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return failureResult(QStringLiteral("sandbox-error"),
                             QStringLiteral("Could not write isolated Lisp source: %1")
                                 .arg(candidate.errorString()));
    const QByteArray source = sourceCode.toUtf8();
    if (candidate.write(source) != source.size())
        return failureResult(QStringLiteral("sandbox-error"),
                             QStringLiteral("Could not write complete isolated Lisp source."));
    candidate.close();

    const QString scriptPath = QDir(temporaryDirectory.path()).filePath("runner.lisp");
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return failureResult(QStringLiteral("sandbox-error"),
                             QStringLiteral("Could not write isolated test harness: %1")
                                 .arg(script.errorString()));
    const QString harness = makeHarness(candidatePath, functionName, tests);
    const QByteArray harnessBytes = harness.toUtf8();
    if (script.write(harnessBytes) != harnessBytes.size())
        return failureResult(QStringLiteral("sandbox-error"),
                             QStringLiteral("Could not write complete isolated test harness."));
    script.close();

    const QString sandboxedExecutable =
        QDir(sbclDirectory).filePath(executableInfo.fileName());
    return launchSandboxed(sandboxedExecutable, temporaryDirectory.path(),
                           scriptPath, sbclDirectory, profile);
#endif
}
