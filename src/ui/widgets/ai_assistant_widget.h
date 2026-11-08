#pragma once

#include <QDockWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QComboBox>
#include <QToolButton>
#include <QJsonArray>
#include <QVector>

#include "network/ai_client.h"
#include "code_block_widget.h"
#include "components/chat_message_widget.h"
#include "components/input_text_edit.h"

class QLabel;

class AIAssistantWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit AIAssistantWidget(QWidget *parent = nullptr);
    void appendToInput(const QString &text);

    void explainSelectedCode(const QString &code, AiClient::ExplainContext context);

private slots:
    void onSendClicked();
    void onResponse(const QString &response);
    void onError(const QString &error);

private:
    struct Conversation
    {
        QString title;
        QJsonArray messages;
        bool hasCustomTitle = false;
    };

    void appendTextMessage(const QString &text, ChatMessageWidget::Role role);
    void appendAIMessage(const QString &markdownResponse);
    void createConversation();
    void renameConversation();
    void deleteConversation();
    void switchConversation(int index);
    void loadConversations();
    void saveConversations() const;
    void renderConversation();
    void updateConversationTitle(const QString &firstPrompt);
    void addWaitingIndicator(const QString &text);
    void clearWaitingIndicator();
    void scrollToBottom();

    AiClient *m_client;
    QScrollArea *m_scrollArea;
    QWidget *m_contentWidget;
    QVBoxLayout *m_chatLayout;
    QLabel *m_waitingIndicator = nullptr;

    InputTextEdit *m_inputField;
    QPushButton *m_sendButton;
    QComboBox *m_contextSelector;
    QToolButton* m_newContextButton;
    QToolButton* m_renameContextButton;
    QToolButton* m_deleteContextButton;

    QVector<Conversation> m_conversations;
    int m_currentConversation = 0;

    bool m_isWaiting;
    bool m_isContextDialogOpen = false;
    bool m_isChatRequest = false;
};