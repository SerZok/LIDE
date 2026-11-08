#include "ai_client.h"
#include "app/settings.h"
#include "app/version.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QDateTime>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QSysInfo>
#include <QLibraryInfo>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QThread>
#include <QSettings>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <dxgi.h>
#endif

Q_LOGGING_CATEGORY(aiBenchmarkLog, "lide.ai.benchmark")

AiClient *AiClient::m_instance = nullptr;

AiClient *AiClient::instance()
{
    if (!m_instance)
    {
        m_instance = new AiClient(qApp);
    }
    return m_instance;
}

AiClient::AiClient(QObject *parent)
    : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &AiClient::onReplyFinished);
}

void AiClient::sendRequest(const QJsonArray &conversationMessages)
{
    Settings *settings = Settings::instance();
    QJsonObject json;
    json["model"] = settings->aiModel();
    json["stream"] = false;

    QJsonArray messages;
    messages.append(QJsonObject{{"role", "system"}, {"content", settings->aiSystemPrompt() + "\n\n" + buildChatInstructions() + "\n\n" + buildHyperSpecInstructions()}});
    for (const QJsonValue& message : conversationMessages)
    {
        messages.append(message);
    }
    json["messages"] = messages;

    int inputCharacters = 0;
    for (const QJsonValue &message : messages)
        inputCharacters += message.toObject().value("content").toString().size();

    postRequest(json, RequestType::Chat, inputCharacters);
}

bool AiClient::beginBenchmarkRun(const QString &suiteId, int repetitions, const QJsonArray &tasks)
{
    if (m_benchmarkActive || repetitions < 1 || m_pendingRequests.size() > 0)
        return false;

    m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_runStartedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    m_benchmarkSuiteId = suiteId;
    m_benchmarkRepetitions = repetitions;
    m_benchmarkRequests = QJsonArray();
    m_benchmarkCanceled = false;
    m_benchmarkCompleted = false;
    m_benchmarkActive = true;
    m_benchmarkReportPath.clear();

    Settings *settings = Settings::instance();
    QUrl endpoint(settings->aiEndpoint());
    QString endpointOrigin = endpoint.scheme() + "://" + endpoint.host();
    if (endpoint.port() >= 0)
        endpointOrigin += ":" + QString::number(endpoint.port());

    m_benchmarkSystemPrompt =
        settings->aiSystemPrompt() + "\n\n" + buildChatInstructions() +
        "\n\n" + buildHyperSpecInstructions();
    const QByteArray systemPrompt = m_benchmarkSystemPrompt.toUtf8();
    m_benchmarkMetadata = QJsonObject{
        {"suiteId", m_benchmarkSuiteId},
        {"applicationVersion", APP_VERSION},
        {"repetitionsPerTask", m_benchmarkRepetitions},
        {"temperature", 0.2},
        {"maxOutputTokens", 512},
        {"modelWarmupPolicy", "no explicit warm-up; measured per request"},
        {"model", settings->aiModel()},
        {"endpointOrigin", endpointOrigin},
        {"endpointPath", endpoint.path()},
        {"tasks", tasks},
        {"systemPrompt", m_benchmarkSystemPrompt},
        {"systemPromptSha256", QString::fromLatin1(
            QCryptographicHash::hash(systemPrompt, QCryptographicHash::Sha256).toHex())},
        {"environment", benchmarkEnvironment()}};

    const QString timestamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd'T'HHmmsszzz'Z'");
    const QString fileName = QString("benchmark_%1_%2.json").arg(timestamp, m_runId);
    m_benchmarkReportPath = QDir(benchmarkResultsDirectory()).filePath(fileName);
    if (!writeBenchmarkReport())
    {
        m_benchmarkActive = false;
        m_benchmarkReportPath.clear();
        return false;
    }
    return true;
}

void AiClient::sendBenchmarkRequest(const QString &taskId, const QString &prompt, int repetition)
{
    if (!m_benchmarkActive)
    {
        emit benchmarkRequestFinished(taskId, repetition,
                                      QJsonObject{{"success", false},
                                                  {"error", tr("Бенчмарк не активен.")}});
        return;
    }

    Settings *settings = Settings::instance();
    QJsonObject json{
        {"model", settings->aiModel()},
        {"stream", false},
        {"messages", QJsonArray{
            QJsonObject{{"role", "system"},
                        {"content", m_benchmarkSystemPrompt}},
            QJsonObject{{"role", "user"}, {"content", prompt}}}}};
    QUrl endpoint(settings->aiEndpoint());
    if (endpoint.path().startsWith("/api/"))
        json.insert("options", QJsonObject{
            {"temperature", 0.2},
            {"num_predict", 512}});
    else
    {
        json.insert("temperature", 0.2);
        json.insert("max_tokens", 512);
    }
    int inputCharacters = 0;
    for (const QJsonValue &message : json.value("messages").toArray())
        inputCharacters += message.toObject().value("content").toString().size();
    postRequest(json, RequestType::Benchmark, inputCharacters, taskId, prompt, repetition);
}

void AiClient::endBenchmarkRun(bool canceled)
{
    if (!m_benchmarkActive)
        return;
    m_benchmarkActive = false;
    m_benchmarkCanceled = canceled;
    m_benchmarkCompleted = true;
    writeBenchmarkReport();
}

QString AiClient::benchmarkReportPath() const
{
    return m_benchmarkReportPath;
}

bool AiClient::setBenchmarkEvaluation(const QString &taskId, int repetition,
                                      bool successful, const QString &notes)
{
    if (m_benchmarkReportPath.isEmpty())
        return false;

    for (qsizetype index = 0; index < m_benchmarkRequests.size(); ++index)
    {
        QJsonObject request = m_benchmarkRequests.at(index).toObject();
        if (request.value("taskId").toString() != taskId ||
            request.value("repetition").toInt() != repetition ||
            !request.value("finished").toObject().value("success").toBool())
            continue;

        const QJsonObject previousRequest = request;
        request.insert("manualEvaluation", successful ? "successful" : "unsuccessful");
        request.insert("manualEvaluationNotes", notes.trimmed());
        m_benchmarkRequests.replace(index, request);
        if (writeBenchmarkReport())
            return true;
        m_benchmarkRequests.replace(index, previousRequest);
        return false;
    }

    return false;
}

bool AiClient::setBenchmarkExecutionResult(const QString &taskId, int repetition,
                                           const QJsonObject &result)
{
    if (m_benchmarkReportPath.isEmpty())
        return false;

    for (qsizetype index = 0; index < m_benchmarkRequests.size(); ++index)
    {
        QJsonObject request = m_benchmarkRequests.at(index).toObject();
        if (request.value("taskId").toString() != taskId ||
            request.value("repetition").toInt() != repetition ||
            !request.value("finished").toObject().value("success").toBool())
            continue;

        const QJsonObject previousRequest = request;
        request.insert("sbclTest", result);
        m_benchmarkRequests.replace(index, request);
        if (writeBenchmarkReport())
            return true;
        m_benchmarkRequests.replace(index, previousRequest);
        return false;
    }

    return false;
}

void AiClient::onReplyFinished(QNetworkReply *reply)
{
    if (!reply)
        return;

    if (!m_pendingRequests.contains(reply))
    {
        qWarning() << "AI response received without a matching request";
        reply->deleteLater();
        return;
    }

    const PendingRequest pending = m_pendingRequests.take(reply);
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray responseData = reply->readAll();
    const qint64 elapsedMs = pending.timer.elapsed();
    const QJsonDocument document = QJsonDocument::fromJson(responseData);

    QString responseText;
    QString failureReason;
    QString serverReportedModel;
    qint64 promptTokens = -1;
    qint64 completionTokens = -1;
    qint64 totalTokens = -1;
    qint64 modelLoadDurationNs = -1;
    qint64 promptEvaluationDurationNs = -1;
    qint64 generationDurationNs = -1;
    bool success = reply->error() == QNetworkReply::NoError;

    if (success)
    {
        if (document.isObject())
        {
            const QJsonObject root = document.object();
            serverReportedModel = root.value("model").toString();
            const QJsonObject ollamaMessage = root.value("message").toObject();
            if (ollamaMessage.value("content").isString())
            {
                responseText = ollamaMessage.value("content").toString();
                promptTokens = root.value("prompt_eval_count").toInteger(-1);
                completionTokens = root.value("eval_count").toInteger(-1);
                if (promptTokens >= 0 && completionTokens >= 0)
                    totalTokens = promptTokens + completionTokens;
                modelLoadDurationNs = root.value("load_duration").toInteger(-1);
                promptEvaluationDurationNs = root.value("prompt_eval_duration").toInteger(-1);
                generationDurationNs = root.value("eval_duration").toInteger(-1);
            }
            else
            {
                const QJsonArray choices = root.value("choices").toArray();
                if (!choices.isEmpty())
                {
                    const QJsonObject choice = choices.first().toObject();
                    const QJsonObject message = choice.value("message").toObject();
                    if (message.value("content").isString())
                    {
                        responseText = message.value("content").toString();
                        const QJsonObject usage = root.value("usage").toObject();
                        promptTokens = usage.value("prompt_tokens").toInteger(-1);
                        completionTokens = usage.value("completion_tokens").toInteger(-1);
                        totalTokens = usage.value("total_tokens").toInteger(-1);
                        if (totalTokens < 0 && promptTokens >= 0 && completionTokens >= 0)
                            totalTokens = promptTokens + completionTokens;
                    }
                }
            }

            const QJsonArray choices = root.value("choices").toArray();
            const bool hasOpenAiContent = !choices.isEmpty() &&
                choices.first().toObject().value("message").toObject()
                    .value("content").isString();
            if (!ollamaMessage.value("content").isString() && !hasOpenAiContent)
            {
                success = false;
                failureReason = tr("Сервер вернул ответ в неподдерживаемом формате.");
            }
        }
        else
        {
            success = false;
            failureReason = tr("Сервер вернул некорректный JSON.");
        }
    }

    if (!success && failureReason.isEmpty())
        failureReason = reply->errorString();

    QJsonObject finishedCheckpoint{
        {"timestampUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"success", success},
        {"httpStatus", httpStatus},
        {"elapsedMs", elapsedMs},
        {"responseCharacters", responseText.size()}};
    if (!serverReportedModel.isEmpty())
        finishedCheckpoint.insert("serverReportedModel", serverReportedModel);
#ifdef Q_OS_WIN
    MEMORYSTATUSEX finishedMemoryStatus{};
    finishedMemoryStatus.dwLength = sizeof(finishedMemoryStatus);
    if (GlobalMemoryStatusEx(&finishedMemoryStatus))
    {
        finishedCheckpoint.insert("availablePhysicalMemoryBytes",
                                  static_cast<qint64>(finishedMemoryStatus.ullAvailPhys));
    }
#endif
    if (promptTokens >= 0)
        finishedCheckpoint.insert("promptTokens", promptTokens);
    if (completionTokens >= 0)
        finishedCheckpoint.insert("completionTokens", completionTokens);
    if (totalTokens >= 0)
        finishedCheckpoint.insert("totalTokens", totalTokens);
    if (modelLoadDurationNs >= 0)
        finishedCheckpoint.insert("modelLoadDurationNs", modelLoadDurationNs);
    if (promptEvaluationDurationNs >= 0)
        finishedCheckpoint.insert("promptEvaluationDurationNs",
                                  promptEvaluationDurationNs);
    if (generationDurationNs >= 0)
        finishedCheckpoint.insert("generationDurationNs", generationDurationNs);
    if (completionTokens > 0 && generationDurationNs > 0)
        finishedCheckpoint.insert("generationTokensPerSecond",
                                  completionTokens * 1'000'000'000.0 / generationDurationNs);
    if (completionTokens > 0 && elapsedMs > 0)
        finishedCheckpoint.insert("endToEndTokensPerSecond",
                                  completionTokens * 1000.0 / elapsedMs);
    if (!success)
        finishedCheckpoint.insert("error", failureReason);
    if (pending.type == RequestType::Benchmark)
    {
        finishBenchmarkRequest(pending.requestId, finishedCheckpoint, responseText);
        QJsonObject benchmarkResult = finishedCheckpoint;
        benchmarkResult.insert("response", responseText);
        emit benchmarkRequestFinished(
            pending.benchmarkTaskId, pending.benchmarkRepetition, benchmarkResult);
    }

    if (success)
    {
        responseText = formatHyperSpecLinks(responseText);
        if (pending.type == RequestType::Chat)
            emit responseReady(responseText);
        else if (pending.type == RequestType::Explain)
            emit explainResponseReady(responseText);
    }
    else if (pending.type == RequestType::Chat)
    {
        emit errorOccurred(failureReason);
    }
    else if (pending.type == RequestType::Explain)
    {
        emit explainErrorOccurred(failureReason);
    }

    reply->deleteLater();
}

void AiClient::sendExplainRequest(const QString &selectedText, ExplainContext context)
{
    Settings *settings = Settings::instance();
    QJsonObject json;
    json["model"] = settings->aiModel();
    json["stream"] = false;

    QString teacherInstructions = buildTeacherInstructions(context);
    QJsonArray messages;
    messages.append(QJsonObject{{"role", "system"}, {"content", settings->aiSystemPrompt() + "\n\n" + teacherInstructions}});
    messages.append(QJsonObject{{"role", "user"}, {"content", selectedText}});
    json["messages"] = messages;

    int inputCharacters = 0;
    for (const QJsonValue &message : messages)
        inputCharacters += message.toObject().value("content").toString().size();

    postRequest(json, RequestType::Explain, inputCharacters);
}

void AiClient::postRequest(const QJsonObject &payload, RequestType type, int inputCharacters,
                           const QString &benchmarkTaskId, const QString &benchmarkPrompt,
                           int benchmarkRepetition)
{
    Settings *settings = Settings::instance();
    QUrl url(settings->aiEndpoint());

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (type == RequestType::Benchmark)
        request.setTransferTimeout(300000);

    const QString apiKey = settings->aiApiKey();
    if (!apiKey.isEmpty())
        request.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());

    const QByteArray requestData = QJsonDocument(payload).toJson();
    QElapsedTimer timer;
    timer.start();
    QNetworkReply *reply = m_networkManager->post(request, requestData);

    PendingRequest pending;
    pending.requestId = m_nextRequestId++;
    pending.type = type;
    pending.model = settings->aiModel();
    pending.inputCharacters = inputCharacters;
    pending.requestBytes = requestData.size();
    pending.timer = timer;
    pending.benchmarkTaskId = benchmarkTaskId;
    pending.benchmarkPrompt = benchmarkPrompt;
    pending.benchmarkRepetition = benchmarkRepetition;

    pending.endpointOrigin = url.scheme() + "://" + url.host();
    if (url.port() >= 0)
        pending.endpointOrigin += ":" + QString::number(url.port());

    const QByteArray requestHash =
        QCryptographicHash::hash(requestData, QCryptographicHash::Sha256).toHex();
    pending.startedCheckpoint = QJsonObject{
        {"timestampUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"endpointOrigin", pending.endpointOrigin},
        {"endpointPath", url.path()},
        {"inputCharacters", inputCharacters},
        {"payloadBytes", pending.requestBytes},
        {"requestFingerprintSha256", QString::fromLatin1(requestHash)}};
#ifdef Q_OS_WIN
    MEMORYSTATUSEX startedMemoryStatus{};
    startedMemoryStatus.dwLength = sizeof(startedMemoryStatus);
    if (GlobalMemoryStatusEx(&startedMemoryStatus))
    {
        pending.startedCheckpoint.insert(
            "availablePhysicalMemoryBytes",
            static_cast<qint64>(startedMemoryStatus.ullAvailPhys));
    }
#endif
    m_pendingRequests.insert(reply, pending);
    if (type == RequestType::Benchmark)
        startBenchmarkRequest(pending);
}

void AiClient::startBenchmarkRequest(const PendingRequest &request)
{
    m_benchmarkRequests.append(QJsonObject{
        {"requestId", static_cast<qint64>(request.requestId)},
        {"taskId", request.benchmarkTaskId},
        {"repetition", request.benchmarkRepetition},
        {"prompt", request.benchmarkPrompt},
        {"model", request.model},
        {"started", request.startedCheckpoint},
        {"manualEvaluation", QJsonValue(QJsonValue::Null)},
        {"manualEvaluationNotes", QString()},
        {"finished", QJsonValue(QJsonValue::Null)}});
    writeBenchmarkReport();
}

void AiClient::finishBenchmarkRequest(quint64 requestId, const QJsonObject &finished,
                                      const QString &response)
{
    for (qsizetype index = 0; index < m_benchmarkRequests.size(); ++index)
    {
        QJsonObject request = m_benchmarkRequests.at(index).toObject();
        if (request.value("requestId").toDouble() != static_cast<double>(requestId))
            continue;

        request.insert("finished", finished);
        request.insert("response", response);
        m_benchmarkRequests.replace(index, request);
        writeBenchmarkReport();
        return;
    }

    qWarning() << "AI benchmark request has no started checkpoint:" << requestId;
}

bool AiClient::writeBenchmarkReport()
{
    qint64 totalElapsedMs = 0;
    qint64 totalPromptTokens = 0;
    qint64 totalCompletionTokens = 0;
    qint64 totalTokens = 0;
    int successfulRequests = 0;
    int failedRequests = 0;
    int measuredTokenRequests = 0;
    int evaluationCount = 0;
    int successfulEvaluations = 0;
    int unsuccessfulEvaluations = 0;
    int sbclTestedRequests = 0;
    int sbclPassedRequests = 0;
    int sbclFailedRequests = 0;
    int sbclInfrastructureErrors = 0;
    qint64 generationTokens = 0;
    qint64 generationDurationNs = 0;
    qint64 endToEndTokens = 0;
    qint64 endToEndElapsedMs = 0;

    for (const QJsonValue &requestValue : m_benchmarkRequests)
    {
        const QJsonObject finished = requestValue.toObject().value("finished").toObject();
        if (finished.isEmpty())
            continue;

        totalElapsedMs += finished.value("elapsedMs").toInteger();
        if (finished.value("success").toBool())
            ++successfulRequests;
        else
            ++failedRequests;

        if (finished.contains("promptTokens") && finished.contains("completionTokens"))
        {
            totalPromptTokens += finished.value("promptTokens").toInteger();
            totalCompletionTokens += finished.value("completionTokens").toInteger();
            totalTokens += finished.value("totalTokens").toInteger();
            ++measuredTokenRequests;
        }
        if (finished.contains("completionTokens") &&
            finished.value("generationDurationNs").toInteger() > 0)
        {
            generationTokens += finished.value("completionTokens").toInteger();
            generationDurationNs += finished.value("generationDurationNs").toInteger();
        }
        if (finished.contains("completionTokens") &&
            finished.value("elapsedMs").toInteger() > 0)
        {
            endToEndTokens += finished.value("completionTokens").toInteger();
            endToEndElapsedMs += finished.value("elapsedMs").toInteger();
        }
        const QJsonObject request = requestValue.toObject();
        const QString evaluation = request.value("manualEvaluation").toString();
        if (evaluation == "successful" || evaluation == "unsuccessful")
        {
            ++evaluationCount;
            if (evaluation == "successful")
                ++successfulEvaluations;
            else
                ++unsuccessfulEvaluations;
        }
        const QString sbclStatus =
            request.value("sbclTest").toObject().value("status").toString();
        if (!sbclStatus.isEmpty())
        {
            if (sbclStatus == "passed")
            {
                ++sbclTestedRequests;
                ++sbclPassedRequests;
            }
            else if (sbclStatus == "failed" || sbclStatus == "timeout" ||
                     sbclStatus == "output-limit" || sbclStatus == "no-code")
            {
                ++sbclTestedRequests;
                ++sbclFailedRequests;
            }
            else
            {
                ++sbclInfrastructureErrors;
            }
        }
    }

    const int completedRequests = successfulRequests + failedRequests;
    QJsonObject summary{
        {"requestCount", static_cast<int>(m_benchmarkRequests.size())},
        {"completedRequests", completedRequests},
        {"successfulRequests", successfulRequests},
        {"failedRequests", failedRequests},
        {"canceled", m_benchmarkCanceled},
        {"completed", m_benchmarkCompleted},
        {"totalElapsedMs", static_cast<double>(totalElapsedMs)}};
    if (completedRequests > 0)
        summary.insert("meanElapsedMs", totalElapsedMs / static_cast<double>(completedRequests));
    if (measuredTokenRequests > 0)
    {
        summary.insert("measuredTokenRequests", measuredTokenRequests);
        summary.insert("promptTokens", totalPromptTokens);
        summary.insert("completionTokens", totalCompletionTokens);
        summary.insert("totalTokens", totalTokens);
    }
    if (generationDurationNs > 0)
        summary.insert("aggregateGenerationTokensPerSecond",
                       generationTokens * 1'000'000'000.0 / generationDurationNs);
    if (endToEndElapsedMs > 0)
        summary.insert("aggregateEndToEndTokensPerSecond",
                       endToEndTokens * 1000.0 / endToEndElapsedMs);
    if (evaluationCount > 0)
    {
        summary.insert("manualEvaluationCount", evaluationCount);
        summary.insert("manualSuccessfulCount", successfulEvaluations);
        summary.insert("manualUnsuccessfulCount", unsuccessfulEvaluations);
        summary.insert("manualSuccessRate",
                       successfulEvaluations * 100.0 / evaluationCount);
    }
    if (sbclTestedRequests > 0 || sbclInfrastructureErrors > 0)
    {
        summary.insert("sbclTestedRequests", sbclTestedRequests);
        summary.insert("sbclPassedRequests", sbclPassedRequests);
        summary.insert("sbclFailedRequests", sbclFailedRequests);
        summary.insert("sbclInfrastructureErrors", sbclInfrastructureErrors);
        summary.insert("sbclPassRate",
                       sbclPassedRequests * 100.0 / sbclTestedRequests);
    }

    const QJsonObject report{
        {"schemaVersion", 4},
        {"runId", m_runId},
        {"startedAtUtc", m_runStartedAtUtc},
        {"updatedAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"benchmark", m_benchmarkMetadata},
        {"summary", summary},
        {"requests", m_benchmarkRequests}};
    const QByteArray json = QJsonDocument(report).toJson(QJsonDocument::Indented);
    qCInfo(aiBenchmarkLog).noquote() << "Benchmark report updated:" << m_benchmarkReportPath;

    const QFileInfo fileInfo(m_benchmarkReportPath);
    if (!QDir().mkpath(fileInfo.absolutePath()))
    {
        const QString error = tr("Не удалось создать каталог отчёта: %1")
                                  .arg(fileInfo.absolutePath());
        qCWarning(aiBenchmarkLog).noquote()
            << error;
        emit benchmarkReportError(error);
        return false;
    }

    QSaveFile file(m_benchmarkReportPath);
    if (!file.open(QIODevice::WriteOnly))
    {
        const QString error = tr("Не удалось открыть отчёт %1: %2")
                                  .arg(m_benchmarkReportPath, file.errorString());
        qCWarning(aiBenchmarkLog).noquote()
            << error;
        emit benchmarkReportError(error);
        return false;
    }

    if (file.write(json) != json.size())
    {
        const QString error = tr("Не удалось записать отчёт %1: %2")
                                  .arg(m_benchmarkReportPath, file.errorString());
        qCWarning(aiBenchmarkLog).noquote()
            << error;
        file.cancelWriting();
        emit benchmarkReportError(error);
        return false;
    }

    if (!file.commit())
    {
        const QString error = tr("Не удалось сохранить отчёт %1: %2")
                                  .arg(m_benchmarkReportPath, file.errorString());
        qCWarning(aiBenchmarkLog).noquote()
            << error;
        emit benchmarkReportError(error);
        return false;
    }
    return true;
}

QString AiClient::benchmarkResultsDirectory() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath("ai-benchmark");
}

QJsonObject AiClient::benchmarkEnvironment() const
{
    QJsonObject environment{
        {"os", QSysInfo::productType()},
        {"osVersion", QSysInfo::productVersion()},
        {"kernel", QSysInfo::kernelType()},
        {"kernelVersion", QSysInfo::kernelVersion()},
        {"architecture", QSysInfo::currentCpuArchitecture()},
        {"qtVersion", QLibraryInfo::version().toString()},
        {"logicalCpuCores", QThread::idealThreadCount()}};

#ifdef Q_OS_WIN
    QSettings processorSettings(
        "HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        QSettings::NativeFormat);
    const QString processorName = processorSettings.value("ProcessorNameString").toString().trimmed();
    if (!processorName.isEmpty())
        environment.insert("processorName", processorName);

    MEMORYSTATUSEX memoryStatus{};
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus))
    {
        environment.insert("installedMemoryBytes",
                           static_cast<qint64>(memoryStatus.ullTotalPhys));
        environment.insert("availableMemoryAtStartBytes",
                           static_cast<qint64>(memoryStatus.ullAvailPhys));
    }

    QJsonArray adapters;
    IDXGIFactory1 *factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(
            __uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory))))
    {
        for (UINT index = 0;; ++index)
        {
            IDXGIAdapter1 *adapter = nullptr;
            if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND)
                break;
            if (!adapter)
                continue;

            DXGI_ADAPTER_DESC1 description{};
            if (SUCCEEDED(adapter->GetDesc1(&description)) &&
                !(description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
            {
                adapters.append(QJsonObject{
                    {"name", QString::fromWCharArray(description.Description)},
                    {"dedicatedVideoMemoryBytes",
                     static_cast<qint64>(description.DedicatedVideoMemory)},
                    {"sharedSystemMemoryBytes",
                     static_cast<qint64>(description.SharedSystemMemory)}});
            }
            adapter->Release();
        }
        factory->Release();
    }
    if (!adapters.isEmpty())
        environment.insert("gpus", adapters);
#endif
    return environment;
}

QString AiClient::buildTeacherInstructions(ExplainContext context) const
{
    QString instructions = buildHyperSpecInstructions() + R"(
Ты — наставник по Common Lisp (SBCL).
Используй Markdown.
Если получен фрагмент кода без вопроса, объясни его, а не оценивай и не предлагай список тем.
Разбери назначение кода и важные формы/функции; отмечай ошибки или нюансы только если они действительно есть.
В поясняющем тексте делай ключевые имена Common Lisp кликабельными ссылками по правилам ниже.
Код, который цитируешь или предлагаешь, оформляй fenced-блоком с языком lisp.
)";

    switch (context)
    {
    case ExplainContext::Repl:
        instructions += R"(КОНТЕКСТ REPL: Выделенный текст может быть сообщением об ошибке SBCL. Начни с расшифровки сообщения, затем задай наводящий вопрос о том, как бы пользователь это исправил.)";
        break;
    case ExplainContext::Editor:
        instructions += R"(КОНТЕКСТ РЕДАКТОРА: Выделенный текст — это код из файла. Разбери его по частям.)";
        break;
    }
    return instructions;
}

QString AiClient::buildChatInstructions() const
{
    return R"(
Правила ответа на сообщения пользователя:
- Если пользователь прислал код без вопроса, считай это просьбой объяснить код.
  Сначала в одном предложении опиши назначение, затем кратко разбери основные разделы и важные формы/функции.
- Не ограничивайся похвалой, пересказом очевидного или приглашением задать вопрос.
  Не предлагай примеры возможных вопросов вместо ответа по присланному коду.
- Указывай вывод или ошибки только если можешь достоверно их определить; не выдумывай поведение.
- В поясняющем тексте выделяй ключевые имена Common Lisp ссылками на HyperSpec согласно следующим правилам.
- Код пользователя не переписывай без просьбы; любой код в ответе оформляй Markdown-блоком с языком lisp.
)";
}

QString AiClient::buildHyperSpecInstructions() const
{
    return R"(
Обязательные правила ссылок на Common Lisp HyperSpec:
- Ставь Markdown-ссылку на HyperSpec при первом упоминании каждой ключевой функции,
  макроса, специальной формы или переменной Common Lisp в поясняющем тексте.
- Для обычного фрагмента кода с несколькими разными символами добавь не менее трёх ссылок,
  если для них можно указать правильные страницы. Не нужно ссылать каждое повторное упоминание.
- Ссылка должна быть Markdown-ссылкой с точным маркером __CLHS_PREFIX__ в начале адреса.
  Пример оформления:
[mapcar](__CLHS_PREFIX__/Body/f_mapcar.htm)
[setf](__CLHS_PREFIX__/Body/m_setf.htm)
- Указывай правильную страницу и имя файла HyperSpec для конкретного символа;
  не угадывай имя файла по написанию символа. Не оформляй символы внутри блока кода ссылками.
- Не оставляй маркер __CLHS_PREFIX__ в тексте ссылки: приложение подставит базовый адрес документации.
- Перед отправкой проверь, что ответ действительно содержит Markdown-ссылки на ключевые символы;
  если поясняющий текст называет символ без ссылки, исправь ответ.
)";
}

QString AiClient::formatHyperSpecLinks(const QString &markdown) const
{
    Settings *settings = Settings::instance();
    QString localPath = settings->localHyperSpecPath();

    const QString onlineBaseUrl = "https://www.lispworks.com/documentation/HyperSpec";
    const QRegularExpression linkPattern(R"((?:__CLHS_PREFIX__|CLHS_PREFIX)(/[^)\s]+))");
    QRegularExpressionMatchIterator matches = linkPattern.globalMatch(markdown);

    QString result;
    int lastPosition = 0;
    while (matches.hasNext())
    {
        const QRegularExpressionMatch match = matches.next();
        const QString relativePath = match.captured(1);
        QString linkUrl = onlineBaseUrl + relativePath;

        if (!localPath.isEmpty())
        {
            const QString localFilePath = QDir(localPath).filePath(relativePath.mid(1));
            const QFileInfo localFile(localFilePath);
            if (localFile.isFile())
            {
                linkUrl = QUrl::fromLocalFile(localFile.absoluteFilePath()).toString(QUrl::FullyEncoded);
            }
        }

        result += markdown.mid(lastPosition, match.capturedStart() - lastPosition);
        result += linkUrl;
        lastPosition = match.capturedEnd();
    }

    result += markdown.mid(lastPosition);
    return result;
}