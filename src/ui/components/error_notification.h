#pragma once

#include <QLabel>
#include <QTimer>
#include <QEvent>
#include <QToolButton>
#include <QPropertyAnimation>
#include <QEnterEvent>

class ErrorNotification : public QLabel {
    Q_OBJECT

public:
    explicit ErrorNotification(QWidget* parent = nullptr);
    void showError(const QString& message, int durationMs = 10000);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

    bool eventFilter(QObject* watched, QEvent* event) override;
    void repositionToCorner();

private:
    QPropertyAnimation* m_fadeIn, * m_fadeOut, * m_buttonFade;
    QToolButton* m_closeButton;
    QTimer* m_autoHideTimer, * m_hideButtonTimer;
};