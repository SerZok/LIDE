#include "ai_client.h"
#include "app/settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

AiClient *AiClient::m_instance = nullptr;

AiClient *AiClient::instance()
{
    if (!m_instance)
    {
        m_instance = new AiClient(qApp);
    }
    return m_instance;
}

AiClient::AiClient(QObject *parent) : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &AiClient::onReplyFinished);
}

void AiClient::sendRequest(const QString &prompt)
{
    m_currentRequestType = RequestType::Chat;

    Settings *settings = Settings::instance();
    QUrl url(settings->aiEndpoint());

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QString apiKey = settings->aiApiKey();
    if (!apiKey.isEmpty())
    {
        request.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());
    }

    QJsonObject json;
    json["model"] = settings->aiModel();
    json["stream"] = false;

    QJsonArray messages;
    messages.append(QJsonObject{{"role", "system"}, {"content", settings->aiSystemPrompt() + "\n\n" + buildChatInstructions() + "\n\n" + buildHyperSpecInstructions()}});
    messages.append(QJsonObject{{"role", "user"}, {"content", prompt}});
    json["messages"] = messages;

    m_networkManager->post(request, QJsonDocument(json).toJson());
}

void AiClient::onReplyFinished(QNetworkReply *reply)
{
    if (!reply)
        return;

    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QString responseText = doc.object().value("message").toObject().value("content").toString();
        responseText = formatHyperSpecLinks(responseText);

        qDebug() << "\n[REPLY]\n"
                 << responseText;

        if (m_currentRequestType == RequestType::Chat)
        {
            emit responseReady(responseText);
        }
        else
        {
            emit explainResponseReady(responseText);
        }
    }
    else
    {
        if (m_currentRequestType == RequestType::Chat)
        {
            emit errorOccurred(reply->errorString());
        }
        else
        {
            emit explainErrorOccurred(reply->errorString());
        }
    }
    reply->deleteLater();
}

void AiClient::sendExplainRequest(const QString &selectedText, ExplainContext context)
{
    m_currentRequestType = RequestType::Explain;

    Settings *settings = Settings::instance();
    QUrl url(settings->aiEndpoint());

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QString apiKey = settings->aiApiKey();
    if (!apiKey.isEmpty())
    {
        request.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());
    }

    QJsonObject json;
    json["model"] = settings->aiModel();
    json["stream"] = false;

    QString teacherInstructions = buildTeacherInstructions(context);
    QJsonArray messages;
    messages.append(QJsonObject{{"role", "system"}, {"content", settings->aiSystemPrompt() + "\n\n" + teacherInstructions}});
    messages.append(QJsonObject{{"role", "user"}, {"content", selectedText}});
    json["messages"] = messages;

    qDebug() << "\n[PROMPT]:\n"
             << messages;

    m_networkManager->post(request, QJsonDocument(json).toJson());
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