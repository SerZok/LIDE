#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>

class AiClient : public QObject {
    Q_OBJECT
public:
    enum class ExplainContext {
        Editor,
        Repl
    };

    static AiClient* instance();

    void sendRequest(const QString& prompt);
    void sendExplainRequest(const QString& selectedText, ExplainContext context);

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
    QNetworkReply* m_currentReply = nullptr;

    enum class RequestType { Chat, Explain };
    RequestType m_currentRequestType = RequestType::Chat;

    explicit AiClient(QObject* parent = nullptr);
    AiClient(const AiClient&) = delete;
    AiClient& operator=(const AiClient&) = delete;

    QString buildChatInstructions() const;
    QString buildHyperSpecInstructions() const;
    QString buildTeacherInstructions(ExplainContext context) const;
    QString formatHyperSpecLinks(const QString& markdown) const;
};