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
    void responseReady(const QString& response);
    void errorOccurred(const QString& error);

private slots:
    //void onReplyFinished();
    void onReplyFinished(QNetworkReply* reply);

private:
    explicit AiClient(QObject* parent = nullptr);
    AiClient(const AiClient&) = delete;
    AiClient& operator=(const AiClient&) = delete;

    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply = nullptr;
    QString m_modelName;

    QString buildTeacherInstructions(ExplainContext context) const;
};