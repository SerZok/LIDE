#pragma once

#include <QDockWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QRegularExpression>

#include "network/ai_client.h"
#include "code_block_widget.h"
#include "components/input_text_edit.h"

class AIAssistantWidget : public QDockWidget {
    Q_OBJECT
public:
    explicit AIAssistantWidget(QWidget* parent = nullptr);
    void appendToInput(const QString& text);

    void explainSelectedCode(const QString& code, AiClient::ExplainContext context);

signals:
    // Сигнал наружу, в главное окно
    void insertCodeToMainEditor(const QString& code); 

private slots:
    void onSendClicked();
    void onResponse(const QString& response);
    void onError(const QString& error);

private:
    void appendTextMessage(const QString& text, bool isUser, bool isError);
    void appendAIMessage(const QString& markdownResponse);

    AiClient* m_client;
    QScrollArea* m_scrollArea;
    QWidget* m_contentWidget;
    QVBoxLayout* m_chatLayout;

    InputTextEdit* m_inputField;
    QPushButton* m_sendButton;

    bool m_isWaiting;
};