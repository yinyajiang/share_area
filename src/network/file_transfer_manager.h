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
    qint64 lastProgressBytes = 0;
    qint64 lastProgressAtMs = 0;
    bool isUpload = false;
    bool headerParsed = false;  // download response header parsed
    QTcpSocket* socket = nullptr;

    // Directory transfer fields
    bool isDirectory = false;
    int totalFiles = 0;
    int completedFiles = 0;
    QStringList pendingRelativePaths;  // upload: files to send
    QString folderRootPath;            // upload: source root; download: dest root
    QString currentRelativePath;       // download: current file's relative path
    qint64 currentFileSize = 0;        // download: current file's size
    qint64 currentFileTransferred = 0; // download: bytes received for current file
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
    void cancelDownload(const QString& fileId);

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
    void cleanupTransfer(QTcpSocket* socket);

private:
    QTcpServer* m_server;
    int m_transferPort;
    QMap<QString, SharedFileInfo> m_localFiles;  // fileId -> info
    QMap<QTcpSocket*, TransferInfo> m_activeTransfers;
    QMap<QTcpSocket*, QByteArray> m_buffers;
    QMap<QTcpSocket*, QByteArray> m_pendingUploadChunks;

    void handleClientRequest(QTcpSocket* socket);
    void handleDownloadResponse(QTcpSocket* socket);
    void sendFileChunk(QTcpSocket* socket);

    // Directory transfer helpers
    void handleDirClientRequest(QTcpSocket* socket, const QString& fileId);
    void sendDirFileChunk(QTcpSocket* socket);
    void handleDirDownloadResponse(QTcpSocket* socket);
    static QStringList enumerateDirFiles(const QString& rootPath);
};
