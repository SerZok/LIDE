#include "ai_client.h"
#include "app/settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDir>

AiClient* AiClient::m_instance = nullptr;

AiClient* AiClient::instance() {
    if (!m_instance) {
        m_instance = new AiClient(qApp);
    }
    return m_instance;
}

AiClient::AiClient(QObject* parent) : QObject(parent) {
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &AiClient::onReplyFinished);
}

void AiClient::sendRequest(const QString& prompt) {
    m_currentRequestType = RequestType::Chat;

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
    messages.append(QJsonObject{ {"role", "system"}, {"content", settings->aiSystemPrompt()} });
    messages.append(QJsonObject{ {"role", "user"}, {"content", prompt} });
    json["messages"] = messages;

    m_networkManager->post(request, QJsonDocument(json).toJson());
}

void AiClient::onReplyFinished(QNetworkReply* reply) {
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QString responseText = doc.object().value("message").toObject().value("content").toString();
        responseText = formatHyperSpecLinks(responseText);

        qDebug() << "\n[REPLY]\n" << responseText;

        if (m_currentRequestType == RequestType::Chat) {
            emit responseReady(responseText);
        }
        else {
            emit explainResponseReady(responseText);
        }
    }
    else {
        if (m_currentRequestType == RequestType::Chat) {
            emit errorOccurred(reply->errorString());
        }
        else {
            emit explainErrorOccurred(reply->errorString());
        }
    }
    reply->deleteLater();
}

void AiClient::sendExplainRequest(const QString& selectedText, ExplainContext context) {
    m_currentRequestType = RequestType::Explain;

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

    QString teacherInstructions = buildTeacherInstructions(context);
    QJsonArray messages;
    messages.append(QJsonObject{ {"role", "system"}, {"content", settings->aiSystemPrompt() + "\n\n" + teacherInstructions} });
    messages.append(QJsonObject{ {"role", "user"}, {"content", selectedText} });
    json["messages"] = messages;

    qDebug() << "\n[PROMPT]:\n" << messages;

    m_networkManager->post(request, QJsonDocument(json).toJson());
}

QString AiClient::buildTeacherInstructions(ExplainContext context) const {
    QString instructions = R"(
Ты — опытный наставник по Common Lisp (SBCL).

ПРАВИЛА ФОРМАТИРОВАНИЯ (Markdown):
1. Отвечай СТРОГО в формате Markdown.
2. Ключевые термины выделяй **жирным**.
3. Примеры кода оборачивай в блоки:
   ```lisp
   (твой код здесь)

ГИПЕРССЫЛКИ НА HYPERSPEC (КРИТИЧЕСКИ ВАЖНО):
Для КАЖДОГО упомянутого символа Common Lisp (функции, макроса, специальной формы, переменной) ты ОБЯЗАН создать Markdown-ссылку.
Используй строгие префиксы файлов HyperSpec:

    Функции: f_<имя>.htm (пример: mapcar)
    Макросы: m_<имя>.htm (пример: setf)
    Специальные формы: s_<имя>.htm (пример: if)
    Переменные: v_<имя>.htm (пример: print-pretty)
    Глоссарий: g_<имя>.htm (пример: cons)

ПРИМЕР ИДЕАЛЬНОГО ОТВЕТА:
Пользователь: Что делает (mapcar #'1+ '(1 2 3))?
Ты:
Выражение использует функцию [mapcar](CLHS_PREFIX/Body/f_mapcar.htm), которая применяет переданную функцию к каждому элементу списка и возвращает новый список.
В данном случае используется функция [1+](CLHS_PREFIX/Body/f_1pl.htm), которая увеличивает число на 1.
Результатом будет новый список (2 3 4).
Попробуй самостоятельно написать аналогичный код, который умножает каждый элемент на 2, используя [mapcar](CLHS_PREFIX/Body/f_mapcar.htm) и функцию умножения [ * ](CLHS_PREFIX/Body/f_st.htm)?
)";

    switch (context) {
    case ExplainContext::Repl:
        instructions += R"(КОНТЕКСТ REPL: Выделенный текст может быть сообщением об ошибке SBCL. Начни с расшифровки сообщения, затем задай наводящий вопрос о том, как бы пользователь это исправил.)";
        break;
    case ExplainContext::Editor:
        instructions += R"(КОНТЕКСТ РЕДАКТОРА: Выделенный текст — это код из файла. Разбери его по частям.)";
        break;
    }
    return instructions;
}

QString AiClient::formatHyperSpecLinks(const QString& html) const {
    Settings* settings = Settings::instance();
    QString localPath = settings->localHyperSpecPath();

    // Если в настройках указан локальный путь. Иначе — официальный онлайн URL
    QString baseUrl = localPath.isEmpty()
        ? "https://lispworks.com/documentation/HyperSpec"
        : QDir::toNativeSeparators(localPath);

    QString result = html;
    result.replace("__CLHS_PREFIX__", baseUrl);
    return result;
}