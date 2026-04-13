#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QFile>
#include "core/shared_file.h"
#include "constants.h"

struct TransferInfo {
    QString fileId;
    QString fileName;
    QString filePath;     // source (upload) or dest (download)
    qint64 fileSize = 0;
    qint64 bytesTransferred = 0;
    bool isUpload = false;
    QTcpSocket* socket = nullptr;
};

class FileTransferManager : public QObject {
    Q_OBJECT

public:
    explicit FileTransferManager(QObject* parent = nullptr);
    ~FileTransferManager();

    bool startServer(int preferredPort = 0);
    void stopServer();
    int transferPort() const;

    void setLocalFiles(const QMap<QString, SharedFileInfo>& files);
    void requestFile(const SharedFileInfo& fileInfo, const QString& savePath,
                     const QHostAddress& peerAddress, int peerPort);

signals:
    void downloadProgress(const QString& fileId, qint64 received, qint64 total);
    void downloadComplete(const QString& fileId, const QString& savePath);
    void downloadError(const QString& fileId, const QString& error);
    void uploadStarted(const QString& fileId);
    void uploadProgress(const QString& fileId, qint64 sent, qint64 total);
    void uploadComplete(const QString& fileId);

private slots:
    void onNewConnection();

private:
    void handleSocketError(QTcpSocket* socket);

private:
    QTcpServer* m_server;
    int m_transferPort;
    QMap<QString, SharedFileInfo> m_localFiles;  // fileId -> info
    QMap<QTcpSocket*, TransferInfo> m_activeTransfers;
    QMap<QTcpSocket*, QByteArray> m_buffers;

    void handleClientRequest(QTcpSocket* socket);
    void handleDownloadResponse(QTcpSocket* socket);
    void sendFileChunk(QTcpSocket* socket);
};
