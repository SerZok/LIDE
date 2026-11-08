#include "ai_assistant_widget.h"
#include "app/settings.h"

#include <QScrollBar>
#include <QLabel>
#include <QRegularExpression>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDebug>
#include <QSignalBlocker>
#include <QMessageBox>
#include <QInputDialog>
#include <QScopedValueRollback>

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

    m_contextSelector = new QComboBox(this);
    m_contextSelector->setObjectName("aiContextSelector");
    m_contextSelector->setToolTip(tr("Контекст"));
    m_contextSelector->setAccessibleName(tr("Контекст"));

    m_newContextButton = new QToolButton(this);
    m_newContextButton->setObjectName("aiNewContextButton");
    m_newContextButton->setText("+");
    m_newContextButton->setToolTip(tr("Новый контекст"));
    m_newContextButton->setAccessibleName(tr("Новый контекст"));
    m_newContextButton->setAutoRaise(true);

    m_renameContextButton = new QToolButton(this);
    m_renameContextButton->setObjectName("aiRenameContextButton");
    m_renameContextButton->setText(QString::fromUtf8("✎"));
    m_renameContextButton->setToolTip(tr("Переименовать"));
    m_renameContextButton->setAccessibleName(tr("Переименовать"));
    m_renameContextButton->setAutoRaise(true);

    m_deleteContextButton = new QToolButton(this);
    m_deleteContextButton->setObjectName("aiDeleteContextButton");
    m_deleteContextButton->setText(QString::fromUtf8("×"));
    m_deleteContextButton->setToolTip(tr("Удалить контекст"));
    m_deleteContextButton->setAccessibleName(tr("Удалить контекст"));
    m_deleteContextButton->setAutoRaise(true);

    QHBoxLayout *contextLayout = new QHBoxLayout();
    contextLayout->setContentsMargins(0, 0, 0, 0);
    contextLayout->setSpacing(4);
    QLabel *contextLabel = new QLabel(tr("Контекст:"), this);
    contextLabel->setObjectName("aiContextLabel");
    contextLayout->addWidget(contextLabel);
    contextLayout->addWidget(m_contextSelector, 1);
    contextLayout->addWidget(m_newContextButton);
    contextLayout->addWidget(m_renameContextButton);
    contextLayout->addWidget(m_deleteContextButton);

    // --- Поле ввода и кнопка ---
    m_inputField = new InputTextEdit(this);
    m_inputField->setObjectName("aiInputField");
    m_inputField->setPlaceholderText(tr("Введите запрос или код..."));

    m_sendButton = new QPushButton(tr("Отправить"), this);
    m_sendButton->setObjectName("aiSendButton");

    // --- Компоновка всего виджета ---
    QWidget *mainWidget = new QWidget(this);
    mainWidget->setObjectName("aiAssistantContent");
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);
    mainLayout->addLayout(contextLayout);
    mainLayout->addWidget(m_scrollArea);

    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->setContentsMargins(2, 0, 2, 2);
    inputLayout->setSpacing(8);
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
    connect(m_newContextButton, &QToolButton::clicked, this, &AIAssistantWidget::createConversation);
    connect(m_renameContextButton, &QToolButton::clicked, this, &AIAssistantWidget::renameConversation);
    connect(m_deleteContextButton, &QToolButton::clicked, this, &AIAssistantWidget::deleteConversation);
    connect(m_contextSelector, QOverload<int>::of(&QComboBox::activated),
            this, &AIAssistantWidget::switchConversation);

    loadConversations();
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

    updateConversationTitle(prompt);
    appendTextMessage(prompt, ChatMessageWidget::Role::User);
    m_conversations[m_currentConversation].messages.append(QJsonObject{{"role", "user"}, {"content", prompt}});
    saveConversations();
    m_isChatRequest = true;
    m_isWaiting = true;

    m_inputField->setEnabled(false);
    m_inputField->setPlaceholderText(tr("Ожидайте ответ..."));
    m_sendButton->setEnabled(false);
    m_contextSelector->setEnabled(false);
    m_newContextButton->setEnabled(false);
    m_renameContextButton->setEnabled(false);
    m_deleteContextButton->setEnabled(false);
    addWaitingIndicator(tr("Думаю..."));

    m_inputField->clear();
    m_client->sendRequest(m_conversations[m_currentConversation].messages);
}

void AIAssistantWidget::onResponse(const QString &response)
{
    m_isWaiting = false;
    clearWaitingIndicator();

    if (m_isChatRequest)
    {
        m_conversations[m_currentConversation].messages.append(QJsonObject{{"role", "assistant"}, {"content", response}});
        saveConversations();
    }
    m_isChatRequest = false;
    appendAIMessage(response);

    m_inputField->setEnabled(true);
    m_inputField->setPlaceholderText(tr("Введите запрос или код..."));
    m_sendButton->setEnabled(true);
    m_contextSelector->setEnabled(true);
    m_newContextButton->setEnabled(true);
    m_renameContextButton->setEnabled(true);
    m_deleteContextButton->setEnabled(true);
    m_inputField->setFocus();
    scrollToBottom();
}

void AIAssistantWidget::onError(const QString &error)
{
    m_isWaiting = false;
    m_isChatRequest = false;
    clearWaitingIndicator();
    appendTextMessage(error, ChatMessageWidget::Role::Error);

    m_sendButton->setEnabled(true);
    m_inputField->setEnabled(true);
    m_contextSelector->setEnabled(true);
    m_newContextButton->setEnabled(true);
    m_renameContextButton->setEnabled(true);
    m_deleteContextButton->setEnabled(true);
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

    appendTextMessage(tr("Объясни выделенный код:\n%1").arg(code), ChatMessageWidget::Role::User);

    m_isWaiting = true;
    m_isChatRequest = false;
    m_inputField->setEnabled(false);
    m_sendButton->setEnabled(false);
    m_contextSelector->setEnabled(false);
    m_newContextButton->setEnabled(false);
    m_renameContextButton->setEnabled(false);
    m_deleteContextButton->setEnabled(false);
    addWaitingIndicator(tr("Анализирую код..."));

    m_client->sendExplainRequest(code, context);
}

void AIAssistantWidget::createConversation()
{
    if (m_isWaiting || m_isContextDialogOpen)
        return;

    Conversation conversation;
    conversation.title = tr("Новый контекст %1").arg(m_conversations.size() + 1);
    m_conversations.append(conversation);

    const QSignalBlocker signalBlocker(m_contextSelector);
    m_contextSelector->addItem(conversation.title);
    m_currentConversation = m_conversations.size() - 1;
    m_contextSelector->setCurrentIndex(m_currentConversation);
    saveConversations();
    renderConversation();
}

void AIAssistantWidget::renameConversation()
{
    if (m_isWaiting || m_isContextDialogOpen ||
        m_currentConversation < 0 || m_currentConversation >= m_conversations.size())
        return;

    const int conversationIndex = m_currentConversation;
    const QString currentTitle = m_conversations[conversationIndex].title;
    QScopedValueRollback<bool> dialogGuard(m_isContextDialogOpen, true);

    bool accepted = false;
    const QString title = QInputDialog::getText(
                              this, tr("Переименование контекста"), tr("Название контекста:"),
                              QLineEdit::Normal, currentTitle, &accepted)
                              .trimmed();
    if (!accepted || title.isEmpty() || title == currentTitle ||
        conversationIndex >= m_conversations.size() ||
        conversationIndex != m_currentConversation ||
        m_conversations[conversationIndex].title != currentTitle)
        return;

    Conversation &conversation = m_conversations[conversationIndex];
    conversation.title = title;
    conversation.hasCustomTitle = true;
    m_contextSelector->setItemText(conversationIndex, title);
    saveConversations();
}

void AIAssistantWidget::deleteConversation()
{
    if (m_isWaiting || m_isContextDialogOpen ||
        m_currentConversation < 0 || m_currentConversation >= m_conversations.size())
        return;

    const int conversationIndex = m_currentConversation;
    const QString title = m_conversations[conversationIndex].title;
    QScopedValueRollback<bool> dialogGuard(m_isContextDialogOpen, true);

    if (QMessageBox::question(this, tr("Удаление контекста"),
                              tr("Удалить контекст «%1»?").arg(title),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }
    if (conversationIndex >= m_conversations.size() ||
        conversationIndex != m_currentConversation ||
        m_conversations[conversationIndex].title != title)
        return;

    m_conversations.removeAt(conversationIndex);

    const QSignalBlocker signalBlocker(m_contextSelector);
    m_contextSelector->removeItem(conversationIndex);
    if (m_conversations.isEmpty())
    {
        Conversation conversation;
        conversation.title = tr("Новый контекст 1");
        m_conversations.append(conversation);
        m_contextSelector->addItem(conversation.title);
    }

    m_currentConversation = qMin(conversationIndex, m_conversations.size() - 1);
    m_contextSelector->setCurrentIndex(m_currentConversation);
    saveConversations();
    renderConversation();
}

void AIAssistantWidget::switchConversation(int index)
{
    if (m_isWaiting || m_isContextDialogOpen ||
        index < 0 || index >= m_conversations.size() || index == m_currentConversation)
        return;

    m_currentConversation = index;
    saveConversations();
    renderConversation();
}

void AIAssistantWidget::loadConversations()
{
    const QString saved = Settings::instance()->aiConversations();
    if (!saved.isEmpty())
    {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(saved.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError)
        {
            qWarning() << "Unable to load AI conversations:" << parseError.errorString();
        }
        else if (!document.isObject())
        {
            qWarning() << "Unable to load AI conversations: saved data is not a JSON object";
        }
        else
        {
            const QJsonObject root = document.object();
            const QJsonArray savedConversations = root.value("conversations").toArray();
            for (const QJsonValue &value : savedConversations)
            {
                const QJsonObject object = value.toObject();
                Conversation conversation;
                conversation.title = object.value("title").toString();
                conversation.hasCustomTitle = object.value("hasCustomTitle").toBool();
                const QJsonArray savedMessages = object.value("messages").toArray();
                for (const QJsonValue &messageValue : savedMessages)
                {
                    const QJsonObject message = messageValue.toObject();
                    const QString role = message.value("role").toString();
                    if ((role == "user" || role == "assistant") && message.value("content").isString())
                        conversation.messages.append(message);
                }

                if (!conversation.title.isEmpty())
                    m_conversations.append(conversation);
            }
            m_currentConversation = root.value("currentIndex").toInt();
        }
    }

    if (m_conversations.isEmpty())
    {
        Conversation conversation;
        conversation.title = tr("Новый контекст 1");
        m_conversations.append(conversation);
    }

    m_currentConversation = qBound(0, m_currentConversation, m_conversations.size() - 1);
    const QSignalBlocker signalBlocker(m_contextSelector);
    for (const Conversation &conversation : m_conversations)
        m_contextSelector->addItem(conversation.title);
    m_contextSelector->setCurrentIndex(m_currentConversation);
    renderConversation();
}

void AIAssistantWidget::saveConversations() const
{
    QJsonArray conversations;
    for (const Conversation &conversation : m_conversations)
    {
        conversations.append(QJsonObject{
            {"title", conversation.title},
            {"hasCustomTitle", conversation.hasCustomTitle},
            {"messages", conversation.messages}});
    }

    const QJsonObject root{
        {"currentIndex", m_currentConversation},
        {"conversations", conversations}};
    Settings::instance()->setAiConversations(
        QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
}

void AIAssistantWidget::renderConversation()
{
    while (QLayoutItem *item = m_chatLayout->takeAt(0))
    {
        QWidget *widget = item->widget();
        delete item;
        delete widget;
    }

    const QJsonArray messages = m_conversations[m_currentConversation].messages;
    for (const QJsonValue &value : messages)
    {
        const QJsonObject message = value.toObject();
        if (message.value("role").toString() == "user")
            appendTextMessage(message.value("content").toString(), ChatMessageWidget::Role::User);
        else
            appendAIMessage(message.value("content").toString());
    }
    scrollToBottom();
}

void AIAssistantWidget::updateConversationTitle(const QString &firstPrompt)
{
    Conversation &conversation = m_conversations[m_currentConversation];
    if (!conversation.messages.isEmpty() || conversation.hasCustomTitle)
        return;

    QString title = firstPrompt.section(QRegularExpression("[\\r\\n]"), 0, 0).trimmed();
    if (title.size() > 32)
        title = title.left(29) + "...";
    if (title.isEmpty())
        title = tr("Новый контекст %1").arg(m_currentConversation + 1);

    conversation.title = title;
    m_contextSelector->setItemText(m_currentConversation, title);
    saveConversations();
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
