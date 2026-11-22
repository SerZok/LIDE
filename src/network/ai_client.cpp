#include "ai_client.h"
#include "app/settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QFileInfo>

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
    messages.append(QJsonObject{{"role", "system"},
                                {"content", settings->aiSystemPrompt() + "\n\n" +
                                                buildChatInstructions()}});
    for (const QJsonValue& message : conversationMessages)
    {
        messages.append(message);
    }
    json["messages"] = messages;

    postRequest(json, RequestType::Chat);
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
    const QByteArray responseData = reply->readAll();
    const QJsonDocument document = QJsonDocument::fromJson(responseData);

    QString responseText;
    QString failureReason;
    bool success = reply->error() == QNetworkReply::NoError;

    if (success)
    {
        if (document.isObject())
        {
            const QJsonObject root = document.object();
            const QJsonObject ollamaMessage = root.value("message").toObject();
            if (ollamaMessage.value("content").isString())
            {
                responseText = ollamaMessage.value("content").toString();
            }
            else
            {
                const QJsonArray choices = root.value("choices").toArray();
                if (!choices.isEmpty())
                {
                    const QJsonObject choice = choices.first().toObject();
                    const QJsonObject message = choice.value("message").toObject();
                    if (message.value("content").isString())
                        responseText = message.value("content").toString();
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

    if (success)
    {
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

void AiClient::sendExplainRequest(const QString &selectedText, ExplainContext context,
                                  const QString &filePath, const QString &fileContents)
{
    Settings *settings = Settings::instance();
    QJsonObject json;
    json["model"] = settings->aiModel();
    json["stream"] = false;

    QString teacherInstructions = buildTeacherInstructions(context);
    QJsonArray messages;
    messages.append(QJsonObject{{"role", "system"}, {"content", settings->aiSystemPrompt() + "\n\n" + teacherInstructions}});
    QString userContent = selectedText;
    if (context == ExplainContext::Editor &&
        (!filePath.isEmpty() || !fileContents.isEmpty()))
    {
        const QString fileName = filePath.isEmpty()
                                     ? tr("Текущий файл")
                                     : QFileInfo(filePath).fileName();
        userContent =
            tr("Файл: %1\nПолное содержимое файла:\n```lisp\n%2\n```\n\n"
               "Выделенный фрагмент для объяснения:\n```lisp\n%3\n```")
                .arg(fileName, fileContents, selectedText);
    }
    messages.append(QJsonObject{{"role", "user"}, {"content", userContent}});
    json["messages"] = messages;

    postRequest(json, RequestType::Explain);
}

void AiClient::postRequest(const QJsonObject &payload, RequestType type)
{
    Settings *settings = Settings::instance();
    QUrl url(settings->aiEndpoint());

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    const QString apiKey = settings->aiApiKey();
    if (!apiKey.isEmpty())
        request.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());

    QNetworkReply *reply = m_networkManager->post(
        request, QJsonDocument(payload).toJson());
    PendingRequest pending;
    pending.type = type;
    m_pendingRequests.insert(reply, pending);
}

QString AiClient::buildTeacherInstructions(ExplainContext context) const
{
    QString instructions = R"(
Ты — наставник по Common Lisp (SBCL).
Используй Markdown.
Если получен фрагмент кода без вопроса, объясни его, а не оценивай и не предлагай список тем.
Разбери назначение кода и важные формы/функции; отмечай ошибки или нюансы только если они действительно есть.
Код, который цитируешь или предлагаешь, оформляй fenced-блоком с языком lisp.
)";

    switch (context)
    {
    case ExplainContext::Repl:
        instructions += R"(КОНТЕКСТ REPL: Выделенный текст может быть сообщением об ошибке SBCL. Начни с расшифровки сообщения, затем задай наводящий вопрос о том, как бы пользователь это исправил.)";
        break;
    case ExplainContext::Editor:
        instructions += R"(КОНТЕКСТ РЕДАКТОРА: Выделенный текст — это код из файла. Разбери его по частям. Если приложено полное содержимое файла, учитывай его для понимания связей и окружения выделенного кода.)";
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
- Если к запросу приложены найденные фрагменты документов, опирайся только на них при ответе о содержании документов.
  Подкрепляй каждое утверждение ссылкой на точный файл и страницу или диапазон строк.
  Если фрагменты не подтверждают ответ, прямо скажи, что в источниках ответа нет; не придумывай цитаты.
- Код пользователя не переписывай без просьбы; любой код в ответе оформляй Markdown-блоком с языком lisp.
)";
}