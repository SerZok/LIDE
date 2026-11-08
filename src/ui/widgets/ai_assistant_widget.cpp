#include "ai_assistant_widget.h"

#include <QScrollBar>
#include <QLabel>
#include <QRegularExpression>
#include <QTimer>

AIAssistantWidget::AIAssistantWidget(QWidget *parent)
    : QDockWidget(tr("AI-Ассистент"), parent), m_isWaiting(false)
{

    setObjectName("AI-assist");

    m_client = AiClient::instance();

    // --- Область чата ---
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setObjectName("aiChatScrollArea");

    m_contentWidget = new QWidget(m_scrollArea);
    m_contentWidget->setObjectName("aiChatContent");

    m_chatLayout = new QVBoxLayout(m_contentWidget);
    m_chatLayout->setAlignment(Qt::AlignTop);
    m_chatLayout->setSpacing(12);
    m_chatLayout->setContentsMargins(8, 8, 8, 8);

    m_scrollArea->setWidget(m_contentWidget);

    // --- Поле ввода и кнопка ---
    m_inputField = new InputTextEdit(this);
    m_inputField->setObjectName("aiInputField");
    m_inputField->setPlaceholderText(tr("Введите запрос или код..."));

    m_sendButton = new QPushButton(tr("Отправить"), this);
    m_sendButton->setObjectName("aiSendButton");

    // --- Компоновка всего виджета ---
    QWidget *mainWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->addWidget(m_scrollArea);

    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->addWidget(m_inputField);
    inputLayout->addWidget(m_sendButton);
    inputLayout->setAlignment(m_sendButton, Qt::AlignBottom);
    mainLayout->addLayout(inputLayout);

    setWidget(mainWidget);

    connect(m_client, &AiClient::responseReady, this, &AIAssistantWidget::onResponse);
    connect(m_client, &AiClient::explainResponseReady, this, &AIAssistantWidget::onResponse);

    connect(m_client, &AiClient::errorOccurred, this, &AIAssistantWidget::onError);
    connect(m_client, &AiClient::explainErrorOccurred, this, &AIAssistantWidget::onError);

    connect(m_inputField, &InputTextEdit::submitRequested, this, &AIAssistantWidget::onSendClicked);
    connect(m_sendButton, &QPushButton::clicked, this, &AIAssistantWidget::onSendClicked);
}

void AIAssistantWidget::appendToInput(const QString &text)
{
    m_inputField->setPlainText(text);
    m_inputField->setFocus();
}

void AIAssistantWidget::appendTextMessage(const QString &text, ChatMessageWidget::Role role)
{
    m_chatLayout->addWidget(new ChatMessageWidget(role, text, m_contentWidget));
}

void AIAssistantWidget::appendAIMessage(const QString &markdownResponse)
{
    QRegularExpression re("```(\\w*)\\r?\\n?([\\s\\S]*?)```");
    QRegularExpressionMatchIterator i = re.globalMatch(markdownResponse);

    int lastPos = 0;

    while (i.hasNext())
    {
        QRegularExpressionMatch match = i.next();

        QString textBefore = markdownResponse.mid(lastPos, match.capturedStart() - lastPos).trimmed();
        if (!textBefore.isEmpty())
        {
            appendTextMessage(textBefore, ChatMessageWidget::Role::Assistant);
        }

        QString lang = match.captured(1).trimmed();
        QString code = match.captured(2).trimmed();

        CodeBlockWidget *codeWidget = new CodeBlockWidget(lang, code, m_contentWidget);
        m_chatLayout->addWidget(codeWidget);

        lastPos = match.capturedEnd();
    }

    QString textAfter = markdownResponse.mid(lastPos).trimmed();
    if (!textAfter.isEmpty())
    {
        appendTextMessage(textAfter, ChatMessageWidget::Role::Assistant);
    }
}

void AIAssistantWidget::onSendClicked()
{
    if (m_isWaiting)
    {
        return;
    }

    QString prompt = m_inputField->toPlainText().trimmed();
    if (prompt.isEmpty())
        return;

    appendTextMessage(prompt, ChatMessageWidget::Role::User);
    m_isWaiting = true;

    m_inputField->setEnabled(false);
    m_inputField->setPlaceholderText(tr("Ожидайте ответ..."));
    m_sendButton->setEnabled(false);
    addWaitingIndicator(tr("Думаю..."));

    m_inputField->clear();
    m_client->sendRequest(prompt);
}

void AIAssistantWidget::onResponse(const QString &response)
{
    m_isWaiting = false;
    clearWaitingIndicator();

    appendAIMessage(response);

    m_inputField->setEnabled(true);
    m_inputField->setPlaceholderText(tr("Введите запрос или код..."));
    m_sendButton->setEnabled(true);
    m_inputField->setFocus();
    scrollToBottom();
}

void AIAssistantWidget::onError(const QString &error)
{
    m_isWaiting = false;
    clearWaitingIndicator();
    appendTextMessage(error, ChatMessageWidget::Role::Error);

    m_sendButton->setEnabled(true);
    m_inputField->setEnabled(true);
    m_inputField->setPlaceholderText(tr("Введите запрос или код..."));
    m_inputField->setFocus();
    scrollToBottom();
}

void AIAssistantWidget::explainSelectedCode(const QString &code, AiClient::ExplainContext context)
{
    if (m_isWaiting)
    {
        return;
    }

    QString contextStr = (context == AiClient::ExplainContext::Repl) ? tr("REPL") : tr("Редактор");

    appendTextMessage(tr("Объясни выделенный код из %1:\n\n%2").arg(contextStr, code),
                      ChatMessageWidget::Role::User);

    m_isWaiting = true;
    m_inputField->setEnabled(false);
    m_sendButton->setEnabled(false);
    addWaitingIndicator(tr("Анализирую код..."));

    m_client->sendExplainRequest(code, context);
}

void AIAssistantWidget::addWaitingIndicator(const QString &text)
{
    m_waitingIndicator = new QLabel(text, m_contentWidget);
    m_waitingIndicator->setObjectName("thinkingLabel");
    m_waitingIndicator->setWordWrap(true);
    m_chatLayout->addWidget(m_waitingIndicator);
    scrollToBottom();
}

void AIAssistantWidget::clearWaitingIndicator()
{
    if (!m_waitingIndicator)
    {
        return;
    }

    m_chatLayout->removeWidget(m_waitingIndicator);
    delete m_waitingIndicator;
    m_waitingIndicator = nullptr;
}

void AIAssistantWidget::scrollToBottom()
{
    QTimer::singleShot(0, m_scrollArea, [this]()
                       {
        QScrollBar* bar = m_scrollArea->verticalScrollBar();
        bar->setValue(bar->maximum()); });
}
