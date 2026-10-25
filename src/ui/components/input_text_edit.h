#pragma once

#include <QTextEdit>

class InputTextEdit : public QTextEdit {
    Q_OBJECT
public:
    explicit InputTextEdit(QWidget* parent = nullptr);

signals:
    void submitRequested();

private slots:
    void adjustHeight();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    int m_minHeight = 40;
    int m_maxHeight = 160;
};