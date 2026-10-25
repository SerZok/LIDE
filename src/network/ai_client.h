#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>

class AiClient : public QObject {
    Q_OBJECT
public:
    explicit AiClient(QObject* parent = nullptr);

    void sendRequest(const QString& prompt);

signals:
    void responseReady(const QString& response);
    void errorOccurred(const QString& error);

private slots:
    //void onReplyFinished();
    void onReplyFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_networkManager;
    QString m_modelName = "qwen2.5-coder:1.5b";
};