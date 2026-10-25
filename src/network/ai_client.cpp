#include "ai_client.h"
#include "app/settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

AiClient::AiClient(QObject* parent) : QObject(parent) {
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &AiClient::onReplyFinished);
}

void AiClient::sendRequest(const QString& prompt) {
    Settings* settings = Settings::instance();

    QUrl url(settings->aiEndpoint());
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QString apiKey = settings->aiApiKey();
    if (!apiKey.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());
    }

    QJsonObject json;
    auto model = settings->aiModel();
    json["model"] = model;
    json["stream"] = false;

    QJsonArray messages;
    QJsonObject systemMessage;
    systemMessage["role"] = "system";
    systemMessage["content"] = settings->aiSystemPrompt();
    messages.append(systemMessage);

    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = prompt;
    messages.append(userMessage);
    json["messages"] = messages;

    m_networkManager->post(request, QJsonDocument(json).toJson());
}

void AiClient::onReplyFinished(QNetworkReply* reply) {
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QString responseText = doc.object().value("message").toObject().value("content").toString();
        qDebug() << "[ai]:" << responseText;
        emit responseReady(responseText);
    }
    else {
        qDebug() << "[ai]:" << reply->errorString();
        emit errorOccurred(reply->errorString());
    }
    reply->deleteLater();
}