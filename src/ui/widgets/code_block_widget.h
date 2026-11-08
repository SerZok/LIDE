#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QClipboard>
#include <QGuiApplication>
#include <QTimer>

class CodeBlockWidget : public QWidget {
    Q_OBJECT
public:
    explicit CodeBlockWidget(const QString& language, const QString& code, QWidget* parent = nullptr);

signals:
private slots:
    void onCopyClicked();

private:
    void updateCodeHeight();
    
    QString m_code;
    QTextEdit* m_codeEdit;
};