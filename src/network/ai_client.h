#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QElapsedTimer>
#include <QUuid>
#include <QString>

class AiClient : public QObject {
    Q_OBJECT
public:
    enum class ExplainContext {
        Editor,
        Repl
    };

    static AiClient* instance();

    void sendRequest(const QJsonArray& conversationMessages);
    void sendExplainRequest(const QString& selectedText, ExplainContext context);
    bool beginBenchmarkRun(const QString &suiteId, int repetitions, const QJsonArray &tasks);
    void sendBenchmarkRequest(const QString &taskId, const QString &prompt, int repetition);
    void endBenchmarkRun(bool canceled);
    QString benchmarkReportPath() const;
    bool setBenchmarkEvaluation(const QString &taskId, int repetition,
                                bool successful, const QString &notes);
    bool setBenchmarkExecutionResult(const QString &taskId, int repetition,
                                     const QJsonObject &result);

signals:
    // Для чата (слушает AIAssistantWidget)
    void responseReady(const QString& response);
    void errorOccurred(const QString& error);

    // Для объяснения
    void explainResponseReady(const QString& response);
    void explainErrorOccurred(const QString& error);
    void benchmarkRequestFinished(const QString &taskId, int repetition,
                                  const QJsonObject &result);
    void benchmarkReportError(const QString &error);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    static AiClient* m_instance;
    QNetworkAccessManager* m_networkManager;
    enum class RequestType { Chat, Explain, Benchmark };
    struct PendingRequest
    {
        quint64 requestId = 0;
        RequestType type = RequestType::Chat;
        QString model;
        int inputCharacters = 0;
        qint64 requestBytes = 0;
        QString endpointOrigin;
        QString benchmarkTaskId;
        QString benchmarkPrompt;
        int benchmarkRepetition = 0;
        QJsonObject startedCheckpoint;
        QElapsedTimer timer;
    };

    QHash<QNetworkReply*, PendingRequest> m_pendingRequests;
    QJsonObject m_benchmarkMetadata;
    QJsonArray m_benchmarkRequests;
    quint64 m_nextRequestId = 1;
    QString m_runId;
    QString m_runStartedAtUtc;
    QString m_benchmarkReportPath;
    QString m_benchmarkSuiteId;
    QString m_benchmarkSystemPrompt;
    int m_benchmarkRepetitions = 0;
    bool m_benchmarkActive = false;
    bool m_benchmarkCanceled = false;
    bool m_benchmarkCompleted = false;

    explicit AiClient(QObject* parent = nullptr);
    AiClient(const AiClient&) = delete;
    AiClient& operator=(const AiClient&) = delete;

    QString buildChatInstructions() const;
    QString buildHyperSpecInstructions() const;
    QString buildTeacherInstructions(ExplainContext context) const;
    QString formatHyperSpecLinks(const QString &markdown) const;
    void postRequest(const QJsonObject &payload, RequestType type, int inputCharacters,
                     const QString &benchmarkTaskId = QString(),
                     const QString &benchmarkPrompt = QString(),
                     int benchmarkRepetition = 0);
    void startBenchmarkRequest(const PendingRequest &request);
    void finishBenchmarkRequest(quint64 requestId, const QJsonObject &finished,
                                const QString &response);
    QJsonObject benchmarkEnvironment() const;
    bool writeBenchmarkReport();
    QString benchmarkResultsDirectory() const;
};