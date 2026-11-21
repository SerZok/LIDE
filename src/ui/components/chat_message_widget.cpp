#include "chat_message_widget.h"
#include "app/settings.h"
#include "context_color.h"

#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QColor>
#include <QFileInfo>
#include <QPalette>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include <QShortcut>
#include <QtMath>

ChatMessageWidget::ChatMessageWidget(Role role, const QString &text, QWidget *parent)
    : QWidget(parent),
      m_bubble(new QWidget(this)),
      m_roleLabel(new QLabel(this)),
      m_textBrowser(new QTextBrowser(this))
{
    QHBoxLayout *rowLayout = new QHBoxLayout(this);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(0);

    QVBoxLayout *bubbleLayout = new QVBoxLayout(m_bubble);
    bubbleLayout->setContentsMargins(14, 10, 14, 10);
    bubbleLayout->setSpacing(4);

    switch (role)
    {
    case Role::User:
        m_bubble->setObjectName("userMessageBubble");
        m_roleLabel->setText(tr("Вы"));
        m_textBrowser->setObjectName("userMessageText");
        rowLayout->addSpacing(12);
        rowLayout->addWidget(m_bubble);
        m_textBrowser->setPlainText(text);
        break;
    case Role::Assistant:
        m_bubble->setObjectName("assistantMessageBubble");
        m_roleLabel->setText(tr("Ассистент"));
        m_textBrowser->setObjectName("assistantMessageText");
        rowLayout->addWidget(m_bubble);
        rowLayout->addSpacing(12);
        m_textBrowser->setMarkdown(text);
        updateLinkStyle();
        m_textBrowser->setOpenExternalLinks(true);
        connect(Settings::instance(), &Settings::settingsChanged, this, &ChatMessageWidget::updateLinkStyle);
        break;
    case Role::Error:
        m_bubble->setObjectName("errorMessageBubble");
        m_roleLabel->setText(tr("Ошибка"));
        m_textBrowser->setObjectName("errorMessageText");
        rowLayout->addWidget(m_bubble);
        rowLayout->addSpacing(12);
        m_textBrowser->setPlainText(text);
        break;
    }

    m_roleLabel->setObjectName("chatMessageRole");
    m_textBrowser->setReadOnly(true);
    m_textBrowser->setFrameShape(QFrame::NoFrame);
    m_textBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textBrowser->setTextInteractionFlags(
        Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse);
    m_textBrowser->setLineWrapMode(QTextEdit::WidgetWidth);
    m_textBrowser->setMinimumSize(0, 0);
    m_textBrowser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_textBrowser->document()->setDocumentMargin(0);

    auto *copyShortcut = new QShortcut(QKeySequence::Copy, m_textBrowser);
    copyShortcut->setContext(Qt::WidgetShortcut);
    connect(copyShortcut, &QShortcut::activated, m_textBrowser, &QTextBrowser::copy);

    bubbleLayout->addWidget(m_roleLabel);
    bubbleLayout->addWidget(m_textBrowser);
    m_bubble->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    updateTextHeight();
}

void ChatMessageWidget::updateLinkStyle()
{
    const bool darkTheme = Settings::instance()->currentTheme() == "dark";
    QPalette palette = m_textBrowser->palette();
    palette.setColor(QPalette::Link, darkTheme ? QColor("#4dd0c8") : QColor("#0067a8"));
    palette.setColor(QPalette::LinkVisited, palette.color(QPalette::Link));
    m_textBrowser->setPalette(palette);
    highlightSourceCitations();
}

void ChatMessageWidget::highlightSourceCitations()
{
    static const QRegularExpression citationPattern(
        R"(\[([^\],\r\n]+),\s*(?:(?:стр\.?|страниц(?:а|ы)?|pages?|p\.?)\s*\d+(?:\s*[-–—]\s*\d+)?|(?:строк(?:а|и)?|lines?)\s+\d+(?:\s*[-–—]\s*\d+)?)\])",
        QRegularExpression::CaseInsensitiveOption);

    const bool darkTheme = Settings::instance()->currentTheme() == "dark";
    const QString renderedText = m_textBrowser->document()->toPlainText();
    QRegularExpressionMatchIterator matches = citationPattern.globalMatch(renderedText);
    while (matches.hasNext())
    {
        const QRegularExpressionMatch match = matches.next();
        const QString fileName = QFileInfo(match.captured(1).trimmed()).fileName();
        QTextCharFormat format;
        format.setForeground(ContextColor::foreground(fileName, darkTheme));
        format.setBackground(ContextColor::background(fileName, darkTheme));
        format.setFontWeight(QFont::DemiBold);
        format.setUnderlineStyle(QTextCharFormat::SingleUnderline);

        QTextCursor cursor(m_textBrowser->document());
        cursor.setPosition(match.capturedStart());
        cursor.setPosition(match.capturedEnd(), QTextCursor::KeepAnchor);
        cursor.mergeCharFormat(format);
    }
}

void ChatMessageWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    QTimer::singleShot(0, this, &ChatMessageWidget::updateTextHeight);
}

void ChatMessageWidget::updateTextHeight()
{
    const int textWidth = m_textBrowser->viewport()->width();
    if (textWidth <= 0)
    {
        return;
    }

    QTextDocument *document = m_textBrowser->document();
    document->setTextWidth(textWidth);
    m_textBrowser->setFixedHeight(qMax(1, qCeil(document->size().height()) + 2));
}
