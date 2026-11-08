#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QResizeEvent;
class QTextBrowser;

class ChatMessageWidget : public QWidget {
    Q_OBJECT
public:
    enum class Role {
        User,
        Assistant,
        Error
    };

    explicit ChatMessageWidget(Role role, const QString& text, QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateTextHeight();

    QWidget* m_bubble;
    QLabel* m_roleLabel;
    QTextBrowser* m_textBrowser;
};
