#include "input_text_edit.h"
#include <QAbstractTextDocumentLayout>
#include <QColor>
#include <QEvent>
#include <QKeyEvent>
#include <QPalette>
#include <QResizeEvent>
#include <QTextDocument>
#include <QTimer>
#include <QFontMetricsF>
#include <QtMath>

InputTextEdit::InputTextEdit(QWidget *parent) : QTextEdit(parent)
{
    setAcceptRichText(false);
    setWordWrapMode(QTextOption::WordWrap);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    document()->setDocumentMargin(0);

    setMinimumHeight(m_minHeight);
    setMaximumHeight(m_maxHeight);
    setFixedHeight(m_minHeight);

    connect(this, &QTextEdit::textChanged, this, &InputTextEdit::adjustHeight);
    QTimer::singleShot(0, this, &InputTextEdit::adjustHeight);
}

void InputTextEdit::adjustHeight()
{
    const int textWidth = viewport()->width();
    if (textWidth <= 0)
    {
        return;
    }

    document()->setTextWidth(textWidth);
    const int documentHeight = qCeil(document()->documentLayout()->documentSize().height());
    const int lineHeight = qCeil(QFontMetricsF(font()).lineSpacing());
    const int contentHeight = qMax(documentHeight, lineHeight);
    const int widgetChromeHeight = height() - viewport()->height();
    const int newHeight = qBound(m_minHeight, contentHeight + widgetChromeHeight, m_maxHeight);
    if (height() != newHeight)
    {
        setFixedHeight(newHeight);
    }
}

void InputTextEdit::resizeEvent(QResizeEvent *event)
{
    QTextEdit::resizeEvent(event);
    QTimer::singleShot(0, this, &InputTextEdit::adjustHeight);
}

void InputTextEdit::changeEvent(QEvent *event)
{
    QTextEdit::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
    {
        QPalette inputPalette = palette();
        const QColor textColor = inputPalette.color(QPalette::Text);
        if (inputPalette.color(QPalette::PlaceholderText) != textColor)
        {
            inputPalette.setColor(QPalette::PlaceholderText, textColor);
            setPalette(inputPalette);
        }
    }
}

void InputTextEdit::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == Qt::NoModifier)
    {
        emit submitRequested();
        return;
    }

    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == Qt::ControlModifier)
    {
        emit submitRequested();
        return;
    }

    QTextEdit::keyPressEvent(event);
}