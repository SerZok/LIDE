#include "ai_assistant_widget.h"
#include "app/settings.h"
#include "components/context_color.h"

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
#include <QScopedValueRollback>
#include <QLineEdit>
#include <QFont>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QStringDecoder>

AIAssistantWidget::AIAssistantWidget(QWidget *parent)
    : QDockWidget(tr("AI-Ассистент"), parent), m_isWaiting(false)
{

    setObjectName("AI-assist");

    m_client = AiClient::instance();
    m_documentRetriever = new DocumentRetriever(this);

    // --- Область чата ---
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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
    m_contextSelector->setEditable(true);
    m_contextSelector->setInsertPolicy(QComboBox::NoInsert);
    m_contextSelector->setToolTip(tr("Выбрать или переименовать чат"));
    m_contextSelector->setAccessibleName(tr("Выбрать или переименовать чат"));
    m_contextSelector->setFixedHeight(26);
    QFont conversationFont = m_contextSelector->font();
    conversationFont.setPointSize(conversationFont.pointSize() + 1);
    m_contextSelector->setFont(conversationFont);

    m_newContextButton = new QToolButton(this);
    m_newContextButton->setObjectName("aiNewContextButton");
    m_newContextButton->setText("+");
    m_newContextButton->setToolTip(tr("Новый чат"));
    m_newContextButton->setAccessibleName(tr("Новый чат"));
    m_newContextButton->setAutoRaise(false);
    m_newContextButton->setFixedSize(24, 24);

    m_deleteContextButton = new QToolButton(this);
    m_deleteContextButton->setObjectName("aiDeleteContextButton");
    m_deleteContextButton->setText(QString::fromUtf8("×"));
    m_deleteContextButton->setToolTip(tr("Удалить чат"));
    m_deleteContextButton->setAccessibleName(tr("Удалить чат"));
    m_deleteContextButton->setAutoRaise(false);
    m_deleteContextButton->setFixedSize(24, 24);

    QHBoxLayout *contextLayout = new QHBoxLayout();
    contextLayout->setContentsMargins(0, 0, 0, 0);
    contextLayout->setSpacing(4);
    contextLayout->addWidget(m_contextSelector, 1);
    contextLayout->addWidget(m_newContextButton);
    contextLayout->addWidget(m_deleteContextButton);

    m_contextFiles = new QWidget(this);
    m_contextFiles->setObjectName("aiContextFiles");
    m_contextFiles->setToolTip(tr("Контекст"));
    m_contextFiles->setAccessibleName(tr("Контекст"));
    m_contextFilesLayout = new FlowLayout(m_contextFiles, 6);

    m_contextLibraryButton = new QToolButton(m_contextFiles);
    m_contextLibraryButton->setObjectName("aiContextLibraryButton");
    m_contextLibraryButton->setText("+");
    m_contextLibraryButton->setToolTip(tr("Выбрать или добавить контекст"));
    m_contextLibraryButton->setAccessibleName(tr("Выбрать или добавить контекст"));
    m_contextLibraryButton->setAccessibleName(tr("Выбрать источники"));
    m_contextLibraryMenu = new QMenu(m_contextLibraryButton);
    m_contextLibraryButton->setMenu(m_contextLibraryMenu);
    m_contextLibraryButton->setPopupMode(QToolButton::InstantPopup);
    m_contextLibraryButton->setFixedSize(24, 24);
    m_contextFilesLayout->addWidget(m_contextLibraryButton);

    // --- Поле ввода и кнопка ---
    QWidget *inputContainer = new QWidget(this);
    inputContainer->setObjectName("aiInputContainer");
    inputContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QGridLayout *inputLayout = new QGridLayout(inputContainer);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setRowStretch(0, 1);
    inputLayout->setColumnStretch(0, 1);

    m_inputField = new InputTextEdit(inputContainer);
    m_inputField->setObjectName("aiInputField");
    m_inputField->setPlaceholderText(tr("Введите запрос или код..."));

    m_sendButton = new QPushButton(inputContainer);
    m_sendButton->setObjectName("aiSendButton");
    m_sendButton->setText(QString::fromUtf8("↑"));
    m_sendButton->setToolTip(tr("Отправить"));
    m_sendButton->setAccessibleName(tr("Отправить"));
    m_sendButton->setFixedSize(24, 24);
    inputLayout->addWidget(m_inputField, 0, 0);
    inputLayout->addWidget(m_sendButton, 0, 0, Qt::AlignRight | Qt::AlignBottom);

    // --- Компоновка всего виджета ---
    QWidget *mainWidget = new QWidget(this);
    mainWidget->setObjectName("aiAssistantContent");
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);
    mainLayout->addLayout(contextLayout);
    mainLayout->addWidget(m_scrollArea, 1);
    mainLayout->addWidget(m_contextFiles, 0);
    mainLayout->addWidget(inputContainer, 0);

    setWidget(mainWidget);

    connect(m_client, &AiClient::responseReady, this, &AIAssistantWidget::onResponse);
    connect(m_client, &AiClient::explainResponseReady, this, &AIAssistantWidget::onResponse);

    connect(m_client, &AiClient::errorOccurred, this, &AIAssistantWidget::onError);
    connect(m_client, &AiClient::explainErrorOccurred, this, &AIAssistantWidget::onError);
    connect(m_documentRetriever, &DocumentRetriever::retrievalReady,
            this, &AIAssistantWidget::onRetrievedContext);
    connect(m_documentRetriever, &DocumentRetriever::retrievalFailed,
            this, &AIAssistantWidget::onError);

    connect(m_inputField, &InputTextEdit::submitRequested, this, &AIAssistantWidget::onSendClicked);
    connect(m_sendButton, &QPushButton::clicked, this, &AIAssistantWidget::onSendClicked);
    connect(m_newContextButton, &QToolButton::clicked, this, &AIAssistantWidget::createConversation);
    connect(m_deleteContextButton, &QToolButton::clicked, this, &AIAssistantWidget::deleteConversation);
    connect(m_contextSelector->lineEdit(), &QLineEdit::editingFinished,
            this, &AIAssistantWidget::renameCurrentConversation);
    connect(m_contextSelector, QOverload<int>::of(&QComboBox::activated),
            this, &AIAssistantWidget::switchConversation);
    connect(m_contextLibraryMenu, &QMenu::aboutToShow,
            this, &AIAssistantWidget::updateContextLibraryMenu);
    connect(Settings::instance(), &Settings::themeChanged,
            this, &AIAssistantWidget::updateContextFilesWidget);

    loadConversations();
    connect(Settings::instance(), &Settings::settingsChanged,
            this, &AIAssistantWidget::refreshKnowledgeFilesFromSettings);
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
    m_deleteContextButton->setEnabled(false);
    setContextFilesEnabled(false);
    const QStringList &contextFiles = m_conversations[m_currentConversation].contextFiles;
    addWaitingIndicator(contextFiles.isEmpty()
                            ? tr("Думаю...")
                            : tr("Ищу в документах..."));

    m_inputField->clear();
    m_documentRetriever->retrieve(contextFiles, prompt);
}

void AIAssistantWidget::onRetrievedContext(const QString &context)
{
    if (!m_isWaiting || !m_isChatRequest)
        return;

    if (m_waitingIndicator)
        m_waitingIndicator->setText(tr("Готовлю ответ..."));

    QJsonArray requestMessages = m_conversations[m_currentConversation].messages;
    if (!context.isEmpty())
    {
        QJsonObject latestMessage = requestMessages.last().toObject();
        latestMessage["content"] = latestMessage.value("content").toString() +
                                   "\n\n" + context;
        requestMessages.replace(requestMessages.size() - 1, latestMessage);
    }
    m_client->sendRequest(requestMessages);
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
    m_deleteContextButton->setEnabled(true);
    setContextFilesEnabled(true);
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
    m_deleteContextButton->setEnabled(true);
    setContextFilesEnabled(true);
    m_inputField->setPlaceholderText(tr("Введите запрос или код..."));
    m_inputField->setFocus();
    scrollToBottom();
}

void AIAssistantWidget::setCurrentEditorFile(const QString &path)
{
    const QString currentFile = path.isEmpty()
                                    ? QString()
                                    : QFileInfo(path).absoluteFilePath();
    if (m_currentEditorFile == currentFile)
        return;

    m_currentEditorFile = currentFile;
    updateContextLibraryMenu();
}

void AIAssistantWidget::explainSelectedCode(const QString &code, AiClient::ExplainContext context,
                                           const QString &filePath,
                                           const QString &fileContents)
{
    if (m_isWaiting)
    {
        return;
    }

    if (context == AiClient::ExplainContext::Editor && !filePath.isEmpty())
        setContextFileSelected(QFileInfo(filePath).absoluteFilePath(), true);

    QString userMessage = tr("Объясни выделенный код:\n%1").arg(code);
    if (context == AiClient::ExplainContext::Editor && !filePath.isEmpty())
        userMessage += tr("\nФайл: %1 (содержимое файла приложено).")
                           .arg(QFileInfo(filePath).fileName());
    else if (context == AiClient::ExplainContext::Editor && !fileContents.isEmpty())
        userMessage += tr("\nСодержимое текущего файла приложено.");
    appendTextMessage(userMessage, ChatMessageWidget::Role::User);

    m_isWaiting = true;
    m_isChatRequest = false;
    m_inputField->setEnabled(false);
    m_sendButton->setEnabled(false);
    m_contextSelector->setEnabled(false);
    m_newContextButton->setEnabled(false);
    m_deleteContextButton->setEnabled(false);
    setContextFilesEnabled(false);
    addWaitingIndicator(tr("Анализирую код..."));

    m_client->sendExplainRequest(code, context, filePath, fileContents);
}

void AIAssistantWidget::createConversation()
{
    if (m_isWaiting || m_isContextDialogOpen)
        return;

    Conversation conversation;
    conversation.title = tr("Новый чат %1").arg(m_conversations.size() + 1);
    conversation.contextFiles = m_knowledgeFiles;
    m_conversations.append(conversation);

    const QSignalBlocker signalBlocker(m_contextSelector);
    m_contextSelector->addItem(conversation.title);
    m_currentConversation = m_conversations.size() - 1;
    m_contextSelector->setCurrentIndex(m_currentConversation);
    saveConversations();
    renderConversation();
}

void AIAssistantWidget::renameCurrentConversation()
{
    if (m_isWaiting || m_currentConversation < 0 ||
        m_currentConversation >= m_conversations.size() ||
        m_contextSelector->currentIndex() != m_currentConversation)
        return;

    const QString title = m_contextSelector->currentText().trimmed();
    Conversation &conversation = m_conversations[m_currentConversation];
    if (title.isEmpty() || title == conversation.title)
    {
        m_contextSelector->setEditText(conversation.title);
        return;
    }

    conversation.title = title;
    conversation.hasCustomTitle = true;
    m_contextSelector->setItemText(m_currentConversation, title);
    m_contextSelector->setEditText(title);
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

    if (QMessageBox::question(this, tr("Удаление чата"),
                              tr("Удалить чат «%1»?").arg(title),
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
        conversation.title = tr("Новый чат 1");
        conversation.contextFiles = m_knowledgeFiles;
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
    m_knowledgeFiles = Settings::instance()->aiKnowledgeFiles();
    bool needsMigration = false;
    QVector<int> conversationsWithoutSelection;
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
            needsMigration = root.contains("knowledgeFiles");
            const QJsonArray savedKnowledgeFiles = root.value("knowledgeFiles").toArray();
            for (const QJsonValue &fileValue : savedKnowledgeFiles)
            {
                if (fileValue.isString())
                {
                    const QString path = fileValue.toString();
                    if (!path.isEmpty() && !m_knowledgeFiles.contains(path))
                        m_knowledgeFiles.append(path);
                }
            }

            const QJsonArray savedConversations = root.value("conversations").toArray();
            for (const QJsonValue &value : savedConversations)
            {
                const QJsonObject object = value.toObject();
                Conversation conversation;
                conversation.title = object.value("title").toString();
                conversation.hasCustomTitle = object.value("hasCustomTitle").toBool();
                const bool hasSavedSelection =
                    object.contains("contextFiles") && object.value("contextFiles").isArray();
                if (hasSavedSelection)
                {
                    const QJsonArray savedContextFiles = object.value("contextFiles").toArray();
                    for (const QJsonValue &fileValue : savedContextFiles)
                    {
                        if (fileValue.isString())
                        {
                            const QString path = fileValue.toString();
                            if (!path.isEmpty())
                            {
                                if (!conversation.contextFiles.contains(path))
                                    conversation.contextFiles.append(path);
                            }
                        }
                    }
                }
                else
                {
                    needsMigration = true;
                }
                const QJsonArray savedMessages = object.value("messages").toArray();
                for (const QJsonValue &messageValue : savedMessages)
                {
                    const QJsonObject message = messageValue.toObject();
                    const QString role = message.value("role").toString();
                    if ((role == "user" || role == "assistant") && message.value("content").isString())
                        conversation.messages.append(message);
                }

                if (!conversation.title.isEmpty())
                {
                    if (!hasSavedSelection)
                        conversationsWithoutSelection.append(m_conversations.size());
                    m_conversations.append(conversation);
                }
            }
            m_currentConversation = root.value("currentIndex").toInt();
        }
    }

    for (int index : conversationsWithoutSelection)
        m_conversations[index].contextFiles = m_knowledgeFiles;

    if (m_conversations.isEmpty())
    {
        Conversation conversation;
        conversation.title = tr("Новый чат 1");
        conversation.contextFiles = m_knowledgeFiles;
        m_conversations.append(conversation);
    }

    m_currentConversation = qBound(0, m_currentConversation, m_conversations.size() - 1);
    const QSignalBlocker signalBlocker(m_contextSelector);
    for (const Conversation &conversation : m_conversations)
        m_contextSelector->addItem(conversation.title);
    m_contextSelector->setCurrentIndex(m_currentConversation);
    renderConversation();
    if (m_knowledgeFiles != Settings::instance()->aiKnowledgeFiles())
    {
        Settings::instance()->setAiKnowledgeFiles(m_knowledgeFiles);
        needsMigration = true;
    }
    if (needsMigration)
    {
        saveConversations();
    }
}

void AIAssistantWidget::saveConversations() const
{
    QJsonArray conversations;
    for (const Conversation &conversation : m_conversations)
    {
        conversations.append(QJsonObject{
            {"title", conversation.title},
            {"hasCustomTitle", conversation.hasCustomTitle},
            {"contextFiles", QJsonArray::fromStringList(conversation.contextFiles)},
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
    updateContextLibraryMenu();
    updateContextFilesWidget();
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

void AIAssistantWidget::addContextFiles()
{
    if (m_isWaiting)
        return;

    const QStringList selectedFiles = QFileDialog::getOpenFileNames(
        this, tr("Добавить файлы контекста"), QString(),
        tr("Документы (*.pdf *.txt *.md *.rst *.html *.htm *.xml *.csv *.tsv *.json *.jsonl *.yaml *.yml *.toml *.ini *.cfg *.conf *.properties *.log *.sql *.lisp *.lsp *.cl *.h *.hh *.hpp *.c *.cc *.cpp *.cxx *.py *.pyi *.js *.jsx *.ts *.tsx *.java *.kt *.go *.rs *.cs *.php *.rb *.swift *.css *.scss *.sh *.bash *.ps1 *.bat *.cmd);;Все файлы (*)"));
    if (selectedFiles.isEmpty())
        return;

    QStringList errors;
    QStringList addedPaths;
    for (const QString &selectedPath : selectedFiles)
    {
        const QString path = QFileInfo(selectedPath).absoluteFilePath();
        if (m_knowledgeFiles.contains(path))
            continue;

        QString error;
        if (QFileInfo(path).suffix().compare("pdf", Qt::CaseInsensitive) == 0)
        {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                error = tr("Не удалось открыть файл %1:\n%2").arg(path, file.errorString());
            else if (file.size() == 0)
                error = tr("Файл %1 пуст.").arg(path);
        }
        else
        {
            QString contents;
            readContextFile(path, contents, error);
        }
        if (!error.isEmpty())
        {
            errors.append(error);
            continue;
        }
        addedPaths.append(path);
    }

    if (!addedPaths.isEmpty())
    {
        QStringList &selectedFiles =
            m_conversations[m_currentConversation].contextFiles;
        for (const QString &path : addedPaths)
        {
            if (!selectedFiles.contains(path))
                selectedFiles.append(path);
        }
        saveConversations();
        updateContextLibraryMenu();
        updateContextFilesWidget();
    }
    if (!errors.isEmpty())
        QMessageBox::warning(this, tr("Ошибка"), errors.join('\n'));
}

void AIAssistantWidget::removeKnowledgeFile(const QString &path)
{
    if (m_isWaiting || m_currentConversation < 0 ||
        m_currentConversation >= m_conversations.size())
        return;

    m_conversations[m_currentConversation].contextFiles.removeAll(path);
    saveConversations();
    updateContextLibraryMenu();
    updateContextFilesWidget();
}

void AIAssistantWidget::refreshKnowledgeFilesFromSettings()
{
    const QStringList knowledgeFiles = Settings::instance()->aiKnowledgeFiles();
    if (knowledgeFiles == m_knowledgeFiles)
        return;

    const QStringList removedKnowledgeFiles =
        m_knowledgeFiles;
    m_knowledgeFiles = knowledgeFiles;
    bool selectionChanged = false;
    for (Conversation &conversation : m_conversations)
    {
        for (const QString &path : removedKnowledgeFiles)
        {
            if (!m_knowledgeFiles.contains(path) &&
                conversation.contextFiles.contains(path))
            {
                conversation.contextFiles.removeAll(path);
                selectionChanged = true;
            }
        }
    }
    if (selectionChanged)
        saveConversations();
    updateContextLibraryMenu();
    updateContextFilesWidget();
}

void AIAssistantWidget::updateContextLibraryMenu()
{
    m_contextLibraryMenu->clear();

    const bool hasCurrentFile = !m_currentEditorFile.isEmpty() &&
                                QFileInfo(m_currentEditorFile).isFile();
    if (hasCurrentFile)
    {
        const QString fileName = QFileInfo(m_currentEditorFile).fileName();
        QAction *currentFileAction =
            m_contextLibraryMenu->addAction(tr("Текущий файл: %1").arg(fileName));
        currentFileAction->setToolTip(
            tr("Добавить или убрать текущий файл из источников: %1").arg(m_currentEditorFile));
        currentFileAction->setCheckable(true);
        const QStringList selectedFiles =
            m_conversations.isEmpty() ? QStringList()
                                      : m_conversations.at(m_currentConversation).contextFiles;
        currentFileAction->setChecked(selectedFiles.contains(m_currentEditorFile));
        connect(currentFileAction, &QAction::toggled, this,
                [this](bool selected)
                { setContextFileSelected(m_currentEditorFile, selected); });
    }

    if (!m_knowledgeFiles.isEmpty())
    {
        if (hasCurrentFile)
            m_contextLibraryMenu->addSeparator();

        const QStringList selectedFiles =
            m_conversations.isEmpty() ? QStringList()
                                      : m_conversations.at(m_currentConversation).contextFiles;
        for (const QString &path : m_knowledgeFiles)
        {
            if (path == m_currentEditorFile)
                continue;
            const QString fileName = QFileInfo(path).fileName();
            QAction *action = m_contextLibraryMenu->addAction(fileName);
            action->setToolTip(path);
            action->setCheckable(true);
            action->setChecked(selectedFiles.contains(path));
            connect(action, &QAction::toggled, this,
                    [this, path](bool selected)
                    { setContextFileSelected(path, selected); });
        }
    }
    else if (!hasCurrentFile)
    {
        QAction *emptyAction = m_contextLibraryMenu->addAction(tr("Откройте файл или добавьте источник"));
        emptyAction->setEnabled(false);
    }

    m_contextLibraryMenu->addSeparator();
    QAction *addAction = m_contextLibraryMenu->addAction(tr("Добавить контекст..."));
    addAction->setEnabled(!m_isWaiting);
    connect(addAction, &QAction::triggered, this, &AIAssistantWidget::addContextFiles);

    const int selectedCount = m_conversations.isEmpty()
                                  ? 0
                                  : m_conversations.at(m_currentConversation).contextFiles.size();
    m_contextLibraryButton->setToolTip(
        tr("Выбрать источники (%1)").arg(selectedCount));
    m_contextLibraryButton->setEnabled(!m_isWaiting);
}

void AIAssistantWidget::setContextFileSelected(const QString &path, bool selected)
{
    if (m_isWaiting || m_currentConversation < 0 ||
        m_currentConversation >= m_conversations.size())
        return;

    QStringList &selectedFiles =
        m_conversations[m_currentConversation].contextFiles;
    if (selected)
    {
        QString error;
        const QFileInfo fileInfo(path);
        if (!fileInfo.exists() || !fileInfo.isFile())
        {
            error = tr("Файл %1 не найден.").arg(path);
        }
        else if (fileInfo.suffix().compare("pdf", Qt::CaseInsensitive) == 0)
        {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                error = tr("Не удалось открыть файл %1:\n%2").arg(path, file.errorString());
            else if (file.size() == 0)
                error = tr("Файл %1 пуст.").arg(path);
        }
        else
        {
            QString contents;
            readContextFile(path, contents, error);
        }

        if (!error.isEmpty())
        {
            QMessageBox::warning(this, tr("Ошибка"), error);
            updateContextLibraryMenu();
            return;
        }
    }

    if (selected && !selectedFiles.contains(path))
        selectedFiles.append(path);
    else if (!selected)
        selectedFiles.removeAll(path);

    saveConversations();
    updateContextFilesWidget();
    m_contextLibraryButton->setToolTip(
        tr("Выбрать источники (%1)").arg(selectedFiles.size()));
}

void AIAssistantWidget::updateContextFilesWidget()
{
    while (m_contextFilesLayout->count() > 1)
    {
        QLayoutItem *item = m_contextFilesLayout->takeAt(1);
        delete item->widget();
        delete item;
    }

    const bool lightTheme = Settings::instance()->currentTheme() == "light";
    const bool darkTheme = !lightTheme;
    if (m_currentConversation < 0 ||
        m_currentConversation >= m_conversations.size())
        return;
    const QStringList &selectedFiles =
        m_conversations[m_currentConversation].contextFiles;

    for (const QString &path : selectedFiles)
    {
        const QString fileName = QFileInfo(path).fileName();
        const QString shortName = fileName.size() > 16
                                      ? fileName.left(13) + "..."
                                      : fileName;

        QFrame *chip = new QFrame(m_contextFiles);
        chip->setObjectName("aiDocumentChip");
        chip->setToolTip(path);
        QHBoxLayout *chipLayout = new QHBoxLayout(chip);
        chipLayout->setContentsMargins(8, 2, 4, 2);
        chipLayout->setSpacing(4);

        QLabel *nameLabel = new QLabel(shortName, chip);
        nameLabel->setObjectName("aiDocumentChipName");
        nameLabel->setToolTip(fileName);
        nameLabel->setMaximumWidth(130);
        chipLayout->addWidget(nameLabel);

        QToolButton *removeButton = new QToolButton(chip);
        removeButton->setObjectName("aiDocumentChipRemoveButton");
        removeButton->setText(QString::fromUtf8("×"));
        removeButton->setToolTip(tr("Убрать документ из текущего контекста"));
        removeButton->setAccessibleName(
            tr("Убрать документ %1 из текущего контекста").arg(fileName));
        removeButton->setAutoRaise(true);
        removeButton->setFixedSize(18, 18);
        chipLayout->addWidget(removeButton);

        const QString background = ContextColor::background(fileName, darkTheme).name();
        chip->setStyleSheet(QStringLiteral(
                                "QFrame#aiDocumentChip { background-color: %1; border: 1px solid %2; "
                                "border-radius: 9px; } "
                                "QLabel#aiDocumentChipName, QToolButton#aiDocumentChipRemoveButton { "
                                "color: %3; } "
                                "QToolButton#aiDocumentChipRemoveButton:hover { background-color: %4; }")
                                .arg(background,
                                     ContextColor::border(fileName, darkTheme).name(),
                                     ContextColor::chipText(darkTheme).name(),
                                     ContextColor::removeHover(fileName, darkTheme).name()));
        m_contextFilesLayout->addWidget(chip);

        connect(removeButton, &QToolButton::clicked, this,
                [this, path]()
                { removeKnowledgeFile(path); });
    }
    m_contextFiles->setVisible(true);
    m_contextFiles->updateGeometry();
}

void AIAssistantWidget::setContextFilesEnabled(bool enabled)
{
    m_contextLibraryButton->setEnabled(enabled);
    m_contextFiles->setEnabled(enabled);
}

bool AIAssistantWidget::readContextFile(const QString &path, QString &content, QString &error) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        error = tr("Не удалось открыть файл %1:\n%2").arg(path, file.errorString());
        return false;
    }

    const QByteArray data = file.readAll();
    if (file.error() != QFile::NoError)
    {
        error = tr("Не удалось прочитать файл %1:\n%2").arg(path, file.errorString());
        return false;
    }
    if (data.contains('\0'))
    {
        error = tr("Файл %1 не является текстовым файлом UTF-8.").arg(path);
        return false;
    }

    QStringDecoder decoder(QStringDecoder::Utf8);
    content = decoder(data);
    if (decoder.hasError())
    {
        error = tr("Файл %1 не является текстовым файлом UTF-8.").arg(path);
        return false;
    }
    return true;
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
        title = tr("Новый чат %1").arg(m_currentConversation + 1);

    conversation.title = title;
    m_contextSelector->setItemText(m_currentConversation, title);
    m_contextSelector->setEditText(title);
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
