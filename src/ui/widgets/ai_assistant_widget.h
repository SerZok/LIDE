#pragma once

#include <QDockWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>

#include "network/ai_client.h"
#include "code_block_widget.h"
#include "components/chat_message_widget.h"
#include "components/input_text_edit.h"

class QLabel;

class AIAssistantWidget : public QDockWidget {
    Q_OBJECT
public:
    explicit AIAssistantWidget(QWidget* parent = nullptr);
    void appendToInput(const QString& text);

    void explainSelectedCode(const QString& code, AiClient::ExplainContext context);

private slots:
    void onSendClicked();
    void onResponse(const QString& response);
    void onError(const QString& error);

private:
    void appendTextMessage(const QString& text, ChatMessageWidget::Role role);
    void appendAIMessage(const QString& markdownResponse);
    void addWaitingIndicator(const QString& text);
    void clearWaitingIndicator();
    void scrollToBottom();

    AiClient* m_client;
    QScrollArea* m_scrollArea;
    QWidget* m_contentWidget;
    QVBoxLayout* m_chatLayout;
    QLabel* m_waitingIndicator = nullptr;

    InputTextEdit* m_inputField;
    QPushButton* m_sendButton;

    bool m_isWaiting;
};