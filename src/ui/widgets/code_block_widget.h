#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QClipboard>
#include <QGuiApplication>

class CodeBlockWidget : public QWidget {
    Q_OBJECT
public:
    explicit CodeBlockWidget(const QString& language, const QString& code, QWidget* parent = nullptr);

signals:
    void insertToEditorRequested(const QString& code);

private slots:
    void onCopyClicked();
    void onInsertClicked();

private:
    QString m_code;
    QPlainTextEdit* m_codeEdit;
};