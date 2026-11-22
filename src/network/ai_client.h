#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>
#include <QHash>
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
    void sendExplainRequest(const QString &selectedText, ExplainContext context,
                            const QString &filePath = QString(),
                            const QString &fileContents = QString());

signals:
    // Для чата (слушает AIAssistantWidget)
    void responseReady(const QString& response);
    void errorOccurred(const QString& error);

    // Для объяснения
    void explainResponseReady(const QString& response);
    void explainErrorOccurred(const QString& error);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    static AiClient* m_instance;
    QNetworkAccessManager* m_networkManager;
    enum class RequestType { Chat, Explain };
    struct PendingRequest
    {
        RequestType type = RequestType::Chat;
    };

    QHash<QNetworkReply*, PendingRequest> m_pendingRequests;

    explicit AiClient(QObject* parent = nullptr);
    AiClient(const AiClient&) = delete;
    AiClient& operator=(const AiClient&) = delete;

    QString buildChatInstructions() const;
    QString buildTeacherInstructions(ExplainContext context) const;
    void postRequest(const QJsonObject &payload, RequestType type);
};