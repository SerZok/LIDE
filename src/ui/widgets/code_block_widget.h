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
    void insertToEditorRequested(const QString& code);

private slots:
    void onCopyClicked();
    void onInsertClicked();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateCodeHeight();
    
    QString m_code;
    QTextEdit* m_codeEdit;
};