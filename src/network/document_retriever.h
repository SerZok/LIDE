#pragma once

#include <QObject>
#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QNetworkAccessManager>
#include <QPair>
#include <QStringList>
#include <QVector>

class QFileInfo;

class DocumentRetriever : public QObject
{
    Q_OBJECT
public:
    explicit DocumentRetriever(QObject *parent = nullptr);

    void retrieve(const QStringList &paths, const QString &question);

signals:
    void retrievalReady(const QString &context);
    void retrievalFailed(const QString &error);

private:
    struct Chunk
    {
        QString source;
        QString location;
        QString text;
        QVector<double> embedding;
    };

    struct IndexedDocument
    {
        qint64 fileSize = -1;
        QDateTime lastModified;
        QByteArray fingerprint;
        QVector<Chunk> chunks;
    };

    QNetworkAccessManager m_networkManager;
    QHash<QString, IndexedDocument> m_documents;
    QVector<QPair<QString, int>> m_pendingChunkReferences;
    QVector<QPair<QString, int>> m_currentBatchReferences;
    QStringList m_currentPaths;
    QString m_question;
    QString m_embeddingEndpoint;
    QString m_embeddingModel;
    QVector<double> m_queryEmbedding;
    int m_pendingChunkOffset = 0;
    bool m_active = false;

    bool loadDocument(const QString &path, IndexedDocument &document, QString &error) const;
    bool calculateFileFingerprint(const QString &path, QByteArray &fingerprint,
                                  QString &error) const;
    QString cacheFilePath(const QString &path) const;
    bool loadCachedDocument(const QString &path, const QFileInfo &info,
                            const QByteArray &fingerprint, IndexedDocument &document) const;
    bool saveCachedDocument(const QString &path, const IndexedDocument &document,
                            QString &error) const;
    void appendTextChunks(const QString &path, const QString &text, int page,
                          IndexedDocument &document) const;
    void embedNextBatch();
    void sendEmbeddingRequest(const QStringList &texts, bool isQuery);
    void handleEmbeddingResponse(QNetworkReply *reply, bool isQuery);
    void rankAndEmit();
    void fail(const QString &error);
};
