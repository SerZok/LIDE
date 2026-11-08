#pragma once

#include <QDockWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QComboBox>
#include <QToolButton>
#include <QJsonArray>
#include <QStringList>
#include <QVector>

#include "network/ai_client.h"
#include "code_block_widget.h"
#include "components/chat_message_widget.h"
#include "components/input_text_edit.h"

class QLabel;
class QListWidget;

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
    void addContextFiles();
    void removeSelectedContextFile();

private:
    struct Conversation
    {
        QString title;
        QJsonArray messages;
        QStringList contextFiles;
        bool hasCustomTitle = false;
    };

    void appendTextMessage(const QString &text, ChatMessageWidget::Role role);
    void appendAIMessage(const QString &markdownResponse);
    void createConversation();
    void renameCurrentConversation();
    void deleteConversation();
    void switchConversation(int index);
    void loadConversations();
    void saveConversations() const;
    void renderConversation();
    void updateConversationTitle(const QString &firstPrompt);
    void updateContextFilesWidget();
    void setContextFilesEnabled(bool enabled);
    bool buildContextFilesPrompt(QString &prompt, QString &error) const;
    bool readContextFile(const QString &path, QString &content, QString &error) const;
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
    QToolButton* m_deleteContextButton;
    QListWidget *m_contextFiles;
    QToolButton *m_addContextFilesButton;
    QToolButton *m_removeContextFileButton;

    QVector<Conversation> m_conversations;
    int m_currentConversation = 0;

    bool m_isWaiting;
    bool m_isContextDialogOpen = false;
    bool m_isChatRequest = false;
};