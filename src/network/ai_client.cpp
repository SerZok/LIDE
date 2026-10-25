#include "ai_client.h"
#include "app/settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

AiClient* AiClient::instance() {
    static AiClient _instance(qApp);
    return &_instance;
}

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

void AiClient::sendExplainRequest(const QString& selectedText, ExplainContext context) {
    Settings* settings = Settings::instance();

    QUrl url(settings->aiEndpoint());
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QString apiKey = settings->aiApiKey();
    if (!apiKey.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());
    }

    QJsonObject json;
    json["model"] = settings->aiModel();
    json["stream"] = false;

    QJsonArray messages;
    QString teacherInstructions = buildTeacherInstructions(context);

    QJsonObject systemMessage;
    systemMessage["role"] = "system";
    systemMessage["content"] = settings->aiSystemPrompt() + "\n\n" + teacherInstructions;
    messages.append(systemMessage);

    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = selectedText;
    messages.append(userMessage);

    json["messages"] = messages;

    m_networkManager->post(request, QJsonDocument(json).toJson());
}

QString AiClient::buildTeacherInstructions(ExplainContext context) const {
    QString instructions = R"(
Ты — дружелюбный и опытный преподаватель Common Lisp (SBCL). 
Твоя цель — объяснить выделенный пользователем текст методически верно, помогая понять суть, а не просто дав ответ.

ПРАВИЛА ФОРМАТИРОВАНИЯ:
1. Отвечай строго в формате HTML, совместимом с QTextBrowser (используй <b>, <p>, <ul>, <li>, <pre><code>). Не используй Markdown.
2. Для КАЖДОЙ упомянутой функции Common Lisp обязательно добавляй гиперссылку на Common Lisp HyperSpec (CLHS). 
   Пример: <a href="https://lispworks.com/documentation/HyperSpec/Body/f_car.htm">car</a>.
)";

    switch (context) {
    case ExplainContext::Repl:
        instructions += R"(
3. КОНТЕКСТ REPL: Выделенный текст может быть как кодом, так и сообщением об ошибке SBCL. 
   Если это ошибка (например, "unbound variable" или "argument is not a list"), сначала объясни простыми словами, что она значит, затем укажи на причину в коде и предложи исправленный вариант.
)";
        break;

    case ExplainContext::Editor:
        instructions += R"(
3. КОНТЕКСТ РЕДАКТОРА: Объясни, что делает этот код, разбери его на части. Если видишь неидиоматичный Lisp-код (например, использование setq вместо setf, или излишнюю императивность), мягко предложи "Lisp-way".)";
        break;
    }

    return instructions;
}