#include "ai_assistant_widget.h"

#include <QApplication>
#include <QScrollBar>
#include <QLabel>

AIAssistantWidget::AIAssistantWidget(QWidget* parent)
    : QDockWidget(tr("AI-Ассистент"), parent), m_isWaiting(false) {

    setObjectName("AI-assist");

    m_client = new AiClient(this);
    connect(m_client, &AiClient::responseReady, this, &AIAssistantWidget::onResponse);
    connect(m_client, &AiClient::errorOccurred, this, &AIAssistantWidget::onError);

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
    m_inputField->setPlaceholderText(tr("Введите запрос или код...\n(Shift+Enter — перенос строки, Enter — отправить)"));

    m_sendButton = new QPushButton(tr("Отправить"), this);
    m_sendButton->setObjectName("aiSendButton");

    connect(m_inputField, &InputTextEdit::submitRequested, this, &AIAssistantWidget::onSendClicked);
    connect(m_sendButton, &QPushButton::clicked, this, &AIAssistantWidget::onSendClicked);

    // --- Компоновка всего виджета ---
    QWidget* mainWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->addWidget(m_scrollArea);

    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->addWidget(m_inputField);
    inputLayout->addWidget(m_sendButton);
    inputLayout->setAlignment(m_sendButton, Qt::AlignBottom);
    mainLayout->addLayout(inputLayout);

    setWidget(mainWidget);
}

void AIAssistantWidget::appendToInput(const QString& text) {
    m_inputField->setPlainText(text);
    m_inputField->setFocus();
}

void AIAssistantWidget::appendTextMessage(const QString& text, bool isUser, bool isError) {
    QTextEdit* textEdit = new QTextEdit(m_contentWidget);

    textEdit->setReadOnly(true);
    textEdit->setFrameShape(QFrame::NoFrame);
    textEdit->setWordWrapMode(QTextOption::WordWrap);
    textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    textEdit->setTextInteractionFlags(Qt::TextSelectableByMouse);

    if (isError) {
        textEdit->setObjectName("errorMessageEdit");
    }
    else if (isUser) {
        textEdit->setObjectName("userMessageEdit");
    }
    else {
        textEdit->setObjectName("aiMessageEdit");
    }

    textEdit->setMarkdown(text);

    // Магия авто-высоты
    textEdit->document()->adjustSize();
    qreal docHeight = textEdit->document()->size().height();
    textEdit->setMaximumHeight(static_cast<int>(docHeight) + 10);
    textEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    m_chatLayout->addWidget(textEdit);
}

void AIAssistantWidget::appendAIMessage(const QString& markdownResponse) {
    QRegularExpression re("```(\\w*)\\r?\\n?([\\s\\S]*?)```");
    QRegularExpressionMatchIterator i = re.globalMatch(markdownResponse);

    int lastPos = 0;

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();

        QString textBefore = markdownResponse.mid(lastPos, match.capturedStart() - lastPos).trimmed();
        if (!textBefore.isEmpty()) {
            appendTextMessage(textBefore, false, false);
        }

        QString lang = match.captured(1).trimmed();
        QString code = match.captured(2).trimmed();

        CodeBlockWidget* codeWidget = new CodeBlockWidget(lang, code, m_contentWidget);
        connect(codeWidget, &CodeBlockWidget::insertToEditorRequested, this, &AIAssistantWidget::insertCodeToMainEditor);
        m_chatLayout->addWidget(codeWidget);

        lastPos = match.capturedEnd();
    }

    QString textAfter = markdownResponse.mid(lastPos).trimmed();
    if (!textAfter.isEmpty()) {
        appendTextMessage(textAfter, false, false);
    }
}

void AIAssistantWidget::onSendClicked() {
    if (m_isWaiting) {
        return;
    }

    QString prompt = m_inputField->toPlainText().trimmed();
    if (prompt.isEmpty()) return;

    appendTextMessage("**Вы:**\n" + prompt, true, false);
    m_isWaiting = true;

    m_inputField->setEnabled(false);
    m_inputField->setPlaceholderText(tr("Ожидайте ответ..."));
    m_sendButton->setEnabled(false);

    QLabel* thinkingLabel = new QLabel(QObject::tr("Думаю..."), m_contentWidget);
    thinkingLabel->setObjectName("thinkingLabel");
    thinkingLabel->setWordWrap(true);
    m_chatLayout->addWidget(thinkingLabel);

    m_inputField->clear();
    m_client->sendRequest(prompt);
}

void AIAssistantWidget::onResponse(const QString& response) {
    m_isWaiting = false;

    // Удаляем метку "Думаю..."
    if (QLayoutItem* item = m_chatLayout->takeAt(m_chatLayout->count() - 1)) {
        delete item->widget();
    }

    appendAIMessage(response);

    m_inputField->setEnabled(true);
    m_inputField->setPlaceholderText(tr("Введите запрос или код...\n(Shift+Enter — перенос строки, Enter — отправить)"));
    m_sendButton->setEnabled(true);
    m_inputField->setFocus();

    QApplication::processEvents();
    QScrollBar* bar = m_scrollArea->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void AIAssistantWidget::onError(const QString& error) {
    m_isWaiting = false;

    if (QLayoutItem* item = m_chatLayout->takeAt(m_chatLayout->count() - 1)) {
        delete item->widget();
    }

    appendTextMessage("**Ошибка:**\n" + error, false, true);

    m_sendButton->setEnabled(true);
    m_inputField->setEnabled(true);
    m_inputField->setFocus();
}