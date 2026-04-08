#include "error_notification.h"

#include <QEvent>

ErrorNotification::ErrorNotification(QWidget* parent) : QLabel(parent) {
    setWordWrap(true);
    setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    hide();

    m_closeButton = new QToolButton(this);
    m_closeButton->setWindowOpacity(0);
    m_closeButton->setObjectName("ErrorNotificationCloseButton");

    m_closeButton->setText("✕");  // или "×", "⨯"
    m_closeButton->setFont(QFont("Segoe UI", 11, QFont::Bold));
    m_closeButton->setIcon(QIcon());
    m_closeButton->setFixedSize(20, 20);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_closeButton->hide();

    connect(m_closeButton, &QToolButton::clicked, this, [this]() {
        m_fadeOut->start();
        if (m_autoHideTimer) m_autoHideTimer->stop();
        });

    m_autoHideTimer = new QTimer(this);
    m_autoHideTimer->setSingleShot(true);
    connect(m_autoHideTimer, &QTimer::timeout, this, [this]() {
        m_fadeOut->start();
        });

    m_fadeIn = new QPropertyAnimation(this, "windowOpacity", this);
    m_fadeIn->setDuration(200);
    m_fadeIn->setStartValue(0);
    m_fadeIn->setEndValue(1);

    m_fadeOut = new QPropertyAnimation(this, "windowOpacity", this);
    m_fadeOut->setDuration(300);
    m_fadeOut->setStartValue(1);
    m_fadeOut->setEndValue(0);
    connect(m_fadeOut, &QPropertyAnimation::finished, this, &QWidget::hide);

    m_hideButtonTimer = new QTimer(this);
    m_hideButtonTimer->setSingleShot(true);
    m_hideButtonTimer->setInterval(150);
    connect(m_hideButtonTimer, &QTimer::timeout, this, [this]() {
        if (!m_closeButton->underMouse() && !underMouse()) {
            m_closeButton->hide();
        }
        });

    m_buttonFade = new QPropertyAnimation(m_closeButton, "windowOpacity", this);
    m_buttonFade->setDuration(150);

    if (parent) {
        parent->installEventFilter(this);
    }
}

bool ErrorNotification::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        if (isVisible()) {
            repositionToCorner();
        }
        return false;
    }
    return QLabel::eventFilter(watched, event);
}

void ErrorNotification::enterEvent(QEnterEvent* event)
{
    QLabel::enterEvent(event);
    m_closeButton->show();
    if (m_hideButtonTimer) m_hideButtonTimer->stop();
}

void ErrorNotification::leaveEvent(QEvent* event)
{
    QLabel::leaveEvent(event);
    if (m_hideButtonTimer) m_hideButtonTimer->start();
}

void ErrorNotification::repositionToCorner() {
    if (!parentWidget()) return;

    int margin = 15;
    int buttonOffset = m_closeButton->isVisible() ? m_closeButton->width() + 5 : 0;

    int x = parentWidget()->width() - width() - margin;
    int y = parentWidget()->height() - height() - margin;
    move(qMax(0, x), qMax(0, y));

    m_closeButton->move(width() - m_closeButton->width() - 5, 3);
}

void ErrorNotification::showError(const QString& message, int durationMs) {
    setText(message);
    adjustSize();
    repositionToCorner();
    show();
    raise();

    m_fadeIn->start();
    m_autoHideTimer->start(durationMs);
}