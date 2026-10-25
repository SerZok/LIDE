#include "input_text_edit.h"
#include <QKeyEvent>
#include <QTextDocument>

InputTextEdit::InputTextEdit(QWidget* parent) : QTextEdit(parent) {
    setAcceptRichText(false);
    setWordWrapMode(QTextOption::WordWrap);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setMinimumHeight(m_minHeight);
    setMaximumHeight(m_maxHeight);
    setFixedHeight(m_minHeight);

    connect(this, &QTextEdit::textChanged, this, &InputTextEdit::adjustHeight);
}

void InputTextEdit::adjustHeight() {
    int docHeight = static_cast<int>(document()->size().height());
    int newHeight = docHeight + 15;
    newHeight = qBound(m_minHeight, newHeight, m_maxHeight);

    setFixedHeight(newHeight);
}

void InputTextEdit::keyPressEvent(QKeyEvent* event) {
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && event->modifiers() == Qt::NoModifier) {
        emit submitRequested();
        return;
    }

    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && event->modifiers() == Qt::ControlModifier) {
        emit submitRequested();
        return;
    }

    QTextEdit::keyPressEvent(event);
}