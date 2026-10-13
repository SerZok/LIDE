#include "ai_assistant_widget.h"
#include <QScrollBar>
#include <QLabel>

AIAssistantWidget::AIAssistantWidget(QWidget* parent)
    : QDockWidget(tr("AI-Ассистент"), parent), m_isWaiting(false) {

    m_client = new OllamaClient(this);
    connect(m_client, &OllamaClient::responseReady, this, &AIAssistantWidget::onResponse);
    connect(m_client, &OllamaClient::errorOccurred, this, &AIAssistantWidget::onError);

    // --- Область чата (ScrollArea вместо QTextEdit) ---
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setObjectName("aiChatScrollArea");

    m_contentWidget = new QWidget(m_scrollArea);
    m_chatLayout = new QVBoxLayout(m_contentWidget);
    m_chatLayout->setAlignment(Qt::AlignTop);
    m_chatLayout->setSpacing(12);
    m_chatLayout->setContentsMargins(8, 8, 8, 8);

    m_scrollArea->setWidget(m_contentWidget);

    // --- Поле ввода и кнопка ---
    m_inputField = new QLineEdit(this);
    m_inputField->setPlaceholderText(tr("Введите запрос или код..."));
    m_inputField->setObjectName("aiInputField");

    m_sendButton = new QPushButton(tr("Отправить"), this);
    m_sendButton->setObjectName("aiSendButton");

    connect(m_sendButton, &QPushButton::clicked, this, &AIAssistantWidget::onSendClicked);
    connect(m_inputField, &QLineEdit::returnPressed, this, &AIAssistantWidget::onSendClicked);

    // --- Компоновка всего виджета ---
    QWidget* mainWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->addWidget(m_scrollArea);

    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->addWidget(m_inputField);
    inputLayout->addWidget(m_sendButton);
    mainLayout->addLayout(inputLayout);

    setWidget(mainWidget);
}

void AIAssistantWidget::appendToInput(const QString& text) {
    m_inputField->setText(text);
    m_inputField->setFocus();
}

// Добавление простого текстового сообщения (User или Error)
void AIAssistantWidget::appendTextMessage(const QString& text, bool isUser) {
    QTextEdit* textEdit = new QTextEdit(m_contentWidget);
    textEdit->setObjectName(isUser ? "userMessageEdit" : "aiMessageEdit");
    textEdit->setReadOnly(true);
    textEdit->setWordWrapMode(QTextOption::WordWrap);
    textEdit->setMarkdown(text);
    textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_chatLayout->addWidget(textEdit);
}

// Парсинг и добавление ответа AI (разбиваем на текст и блоки кода)
void AIAssistantWidget::appendAIMessage(const QString& markdownResponse) {
    // Поиск кода
    QRegularExpression re("```(\\w*)\\r?\\n([\\s\\S]*?)```");
    QRegularExpressionMatchIterator i = re.globalMatch(markdownResponse);

    int lastPos = 0;

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();

        // 1. Добавляем обычный текст ДО блока кода
        QString textBefore = markdownResponse.mid(lastPos, match.capturedStart() - lastPos).trimmed();
        if (!textBefore.isEmpty()) {
            appendTextMessage(textBefore, false);
        }

        // 2. Создаем и добавляем виджет блока кода
        QString lang = match.captured(1).trimmed();
        QString code = match.captured(2).trimmed();

        CodeBlockWidget* codeWidget = new CodeBlockWidget(lang, code, m_contentWidget);
        // Связываем сигнал вставки из блока кода с сигналом всего виджета
        connect(codeWidget, &CodeBlockWidget::insertToEditorRequested, this, &AIAssistantWidget::insertCodeToMainEditor);

        m_chatLayout->addWidget(codeWidget);

        lastPos = match.capturedEnd();
    }

    // 3. Добавляем остаток текста ПОСЛЕ последнего блока кода (или весь текст, если блоков не было)
    QString textAfter = markdownResponse.mid(lastPos).trimmed();
    if (!textAfter.isEmpty()) {
        appendTextMessage(textAfter, false);
    }
}

void AIAssistantWidget::onSendClicked() {
    QString prompt = m_inputField->text().trimmed();
    if (prompt.isEmpty()) return;

    appendTextMessage("**Вы:**\n" + prompt, true);
    m_isWaiting = true;

    // Временная метка "Думает..."
    QLabel* thinkingLabel = new QLabel("**AI:** _Думает..._", m_contentWidget);
    thinkingLabel->setObjectName("thinkingLabel");
    thinkingLabel->setWordWrap(true);
    m_chatLayout->addWidget(thinkingLabel);

    m_inputField->clear();
    m_sendButton->setEnabled(false);
    m_client->sendRequest(prompt);
}

void AIAssistantWidget::onResponse(const QString& response) {
    m_isWaiting = false;

    // Удаляем метку "Думает..."
    if (QLayoutItem* item = m_chatLayout->takeAt(m_chatLayout->count() - 1)) {
        delete item->widget();
    }

    appendAIMessage(response);
    m_sendButton->setEnabled(true);
    m_inputField->setFocus();

    QScrollBar* bar = m_scrollArea->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void AIAssistantWidget::onError(const QString& error) {
    m_isWaiting = false;
    if (QLayoutItem* item = m_chatLayout->takeAt(m_chatLayout->count() - 1)) {
        delete item->widget();
    }

    QTextEdit* errorEdit = new QTextEdit(m_contentWidget);
    errorEdit->setObjectName("errorMessageEdit");
    errorEdit->setReadOnly(true);
    errorEdit->setMarkdown("**Ошибка:**\n" + error);
    errorEdit->setFixedHeight(errorEdit->document()->size().height() + 10);
    errorEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_chatLayout->addWidget(errorEdit);
    m_sendButton->setEnabled(true);
}