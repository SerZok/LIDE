#include "ollama_client.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

OllamaClient::OllamaClient(QObject* parent) : QObject(parent) {
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &OllamaClient::onReplyFinished);
}

void OllamaClient::sendRequest(const QString& prompt) {
    QUrl url("http://localhost:11434/api/chat");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Формируем JSON для Ollama (формат chat)
    QJsonObject json;
    json["model"] = m_modelName;
    json["stream"] = false;

    QJsonArray messages;
    QJsonObject systemMessage;
    systemMessage["role"] = "system";
    systemMessage["content"] = "Ты эксперт по Common Lisp (SBCL)."
        "Отвечай кратко. Весь код обязательно оформляй в блоки markdown с указанием языка,"
        "например: ```lisp\\n(код)\\n```";
    messages.append(systemMessage);

    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = prompt;
    messages.append(userMessage);

    json["messages"] = messages;

    QJsonDocument doc(json);
    m_networkManager->post(request, doc.toJson());
}

void OllamaClient::onReplyFinished(QNetworkReply* reply) {
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QString responseText = doc.object().value("message").toObject().value("content").toString();
        emit responseReady(responseText);
    }
    else {
        emit errorOccurred(reply->errorString());
    }
    reply->deleteLater();
}