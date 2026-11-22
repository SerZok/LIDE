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
#include "network/document_retriever.h"
#include "code_block_widget.h"
#include "components/chat_message_widget.h"
#include "components/flow_layout.h"
#include "components/input_text_edit.h"

class QLabel;
class QMenu;

class AIAssistantWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit AIAssistantWidget(QWidget *parent = nullptr);
    void appendToInput(const QString &text);
    void setCurrentEditorFile(const QString &path);

    void explainSelectedCode(const QString &code, AiClient::ExplainContext context,
                             const QString &filePath = QString(),
                             const QString &fileContents = QString());

private slots:
    void onSendClicked();
    void onResponse(const QString &response);
    void onError(const QString &error);
    void onRetrievedContext(const QString &context);
    void refreshKnowledgeFilesFromSettings();
    void addContextFiles();

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
    void updateContextLibraryMenu();
    void updateContextFilesWidget();
    void setContextFileSelected(const QString &path, bool selected);
    void setContextFilesEnabled(bool enabled);
    void removeKnowledgeFile(const QString &path);
    bool readContextFile(const QString &path, QString &content, QString &error) const;
    void addWaitingIndicator(const QString &text);
    void clearWaitingIndicator();
    void scrollToBottom();

    AiClient *m_client;
    DocumentRetriever *m_documentRetriever;
    QScrollArea *m_scrollArea;
    QWidget *m_contentWidget;
    QVBoxLayout *m_chatLayout;
    QLabel *m_waitingIndicator = nullptr;

    InputTextEdit *m_inputField;
    QPushButton *m_sendButton;
    QComboBox *m_contextSelector;
    QToolButton* m_newContextButton;
    QToolButton* m_deleteContextButton;
    QWidget *m_contextFiles;
    FlowLayout *m_contextFilesLayout;
    QToolButton *m_contextLibraryButton;
    QMenu *m_contextLibraryMenu;

    QVector<Conversation> m_conversations;
    QStringList m_knowledgeFiles;
    int m_currentConversation = 0;
    QString m_currentEditorFile;

    bool m_isWaiting;
    bool m_isContextDialogOpen = false;
    bool m_isChatRequest = false;
};