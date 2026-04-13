#include "file_transfer_manager.h"
#include <QTcpSocket>
#include <QFile>
#include <QDir>
#include <QNetworkProxy>
#include <QDateTime>
#include <array>

namespace {
constexpr qint64 kSocketBufferSize =
    static_cast<qint64>(Constants::TRANSFER_CHUNK_SIZE) * Constants::TRANSFER_PIPELINE_CHUNKS;
constexpr qint64 kDownloadIoBufferSize = 64 * 1024;

bool shouldReportProgress(TransferInfo& info) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool firstReport = info.lastProgressAtMs == 0;
    const bool reachedBytes =
        (info.bytesTransferred - info.lastProgressBytes) >= Constants::TRANSFER_PROGRESS_UPDATE_BYTES;
    const bool reachedTime =
        (now - info.lastProgressAtMs) >= Constants::TRANSFER_PROGRESS_UPDATE_INTERVAL_MS;
    const bool finished = info.fileSize > 0 && info.bytesTransferred >= info.fileSize;

    if (!(firstReport || reachedBytes || reachedTime || finished)) {
        return false;
    }

    info.lastProgressAtMs = now;
    info.lastProgressBytes = info.bytesTransferred;
    return true;
}
}  // namespace

FileTransferManager::FileTransferManager(QObject* parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_transferPort(0)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &FileTransferManager::onNewConnection);
}

FileTransferManager::~FileTransferManager() {
    stopServer();
}

bool FileTransferManager::startServer(int preferredPort) {
    if (m_server->isListening()) {
        m_server->close();
    }

    if (!m_server->listen(QHostAddress::AnyIPv4, preferredPort)) {
        qWarning("Failed to start TCP server on port %d: %s",
                 preferredPort, qUtf8Printable(m_server->errorString()));
        return false;
    }

    m_transferPort = m_server->serverPort();
    qDebug("File transfer server listening on port %d", m_transferPort);
    return true;
}

void FileTransferManager::stopServer() {
    // 关闭所有活跃传输，清理下载中的部分文件
    const QList<QTcpSocket*> sockets = m_activeTransfers.keys();
    for (QTcpSocket* socket : sockets) {
        if (socket) {
            // 删除未完成的下载文件
            if (m_activeTransfers.contains(socket)) {
                const TransferInfo& info = m_activeTransfers[socket];
                if (!info.isUpload && info.bytesTransferred > 0
                    && info.bytesTransferred < info.fileSize
                    && !info.filePath.isEmpty()) {
                    QFile::remove(info.filePath);
                }
            }
            socket->disconnectFromHost();
            cleanupTransfer(socket);
        }
    }

    m_server->close();
    m_transferPort = 0;
}

int FileTransferManager::transferPort() const {
    return m_transferPort;
}

void FileTransferManager::setLocalFiles(const QMap<QString, SharedFileInfo>& files) {
    m_localFiles = files;
}

void FileTransferManager::requestFile(const SharedFileInfo& fileInfo,
                                      const QString& savePath,
                                      const QHostAddress& peerAddress,
                                      int peerPort) {
    qDebug() << "Requesting file:" << fileInfo.fileName
             << "id:" << fileInfo.fileId
             << "from" << peerAddress.toString() << ":" << peerPort
             << "save to:" << savePath;

    QTcpSocket* socket = new QTcpSocket(this);
    socket->setProxy(QNetworkProxy::NoProxy);  // 绕过系统代理
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, kSocketBufferSize);
    socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, kSocketBufferSize);
    socket->setReadBufferSize(kSocketBufferSize);
    TransferInfo info;
    info.fileId = fileInfo.fileId;
    info.fileName = fileInfo.fileName;
    info.filePath = savePath;
    // fileSize 由远端响应头的 OK|size 提供，避免跳过响应头解析
    info.fileSize = 0;
    info.bytesTransferred = 0;
    info.isUpload = false;
    info.socket = socket;

    m_activeTransfers[socket] = info;
    m_buffers[socket] = QByteArray();

    // 连接信号
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        if (!m_activeTransfers.contains(socket)) {
            return;
        }
        if (m_activeTransfers.value(socket).isUpload) {
            handleClientRequest(socket);
        } else {
            handleDownloadResponse(socket);
        }
    });
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, [this, socket](QAbstractSocket::SocketError error) {
        Q_UNUSED(error);
        handleSocketError(socket);
    });
    connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
        cleanupTransfer(socket);
    });

    // 连接成功后发送文件请求
    connect(socket, &QTcpSocket::connected, this, [this, socket]() {
        if (!m_activeTransfers.contains(socket)) {
            return;
        }
        qDebug() << "Connected to peer, sending GET request for"
                 << m_activeTransfers.value(socket).fileId;
        QByteArray request = QStringLiteral("GET|%1\n").arg(
            m_activeTransfers.value(socket).fileId).toUtf8();
        socket->write(request);
    });

    // 连接到远程服务器
    socket->connectToHost(peerAddress, peerPort);
}

void FileTransferManager::cancelDownload(const QString& fileId) {
    // 查找该 fileId 对应的下载 socket
    for (auto it = m_activeTransfers.begin(); it != m_activeTransfers.end(); ++it) {
        QTcpSocket* socket = it.key();
        const TransferInfo& info = it.value();
        if (!info.isUpload && info.fileId == fileId) {
            // 删除部分下载的文件
            if (!info.filePath.isEmpty()) {
                QFile::remove(info.filePath);
            }
            socket->disconnectFromHost();
            cleanupTransfer(socket);
            return;
        }
    }
}

void FileTransferManager::onNewConnection() {
    QTcpSocket* clientSocket = m_server->nextPendingConnection();
    clientSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    clientSocket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, kSocketBufferSize);
    clientSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, kSocketBufferSize);
    clientSocket->setReadBufferSize(kSocketBufferSize);

    // 标记为上传连接，使 readyRead 路由到 handleClientRequest
    TransferInfo info;
    info.isUpload = true;
    info.socket = clientSocket;
    m_activeTransfers[clientSocket] = info;

    connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]() {
        if (!m_activeTransfers.contains(clientSocket)) {
            return;
        }
        if (m_activeTransfers.value(clientSocket).isUpload) {
            handleClientRequest(clientSocket);
        } else {
            handleDownloadResponse(clientSocket);
        }
    });
    connect(clientSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, [this, clientSocket](QAbstractSocket::SocketError error) {
        Q_UNUSED(error);
        handleSocketError(clientSocket);
    });
    connect(clientSocket, &QTcpSocket::disconnected, this, [this, clientSocket]() {
        cleanupTransfer(clientSocket);
    });

    // 初始化空缓冲区
    m_buffers[clientSocket] = QByteArray();

    // 处理在信号连接前已到达的数据
    if (clientSocket->bytesAvailable() > 0) {
        handleClientRequest(clientSocket);
    }
}

void FileTransferManager::handleClientRequest(QTcpSocket* socket) {
    // 接收客户端请求: GET|fileId\n
    QByteArray data = socket->readAll();
    m_buffers[socket].append(data);

    QByteArray buffer = m_buffers[socket];
    int newlineIndex = buffer.indexOf('\n');

    if (newlineIndex == -1) {
        return;  // 等待完整请求
    }

    QByteArray request = buffer.left(newlineIndex);
    m_buffers[socket] = buffer.mid(newlineIndex + 1);

    QString requestStr = QString::fromUtf8(request).trimmed();
    qDebug() << "Server received request:" << requestStr;
    QStringList parts = requestStr.split('|');

    if (parts.size() < 2 || parts[0] != QStringLiteral("GET")) {
        qWarning("Invalid request: %s", qUtf8Printable(requestStr));
        socket->write("ERROR|invalid_request\n");
        socket->flush();
        socket->disconnectFromHost();
        return;
    }

    QString fileId = parts[1];

    if (!m_localFiles.contains(fileId)) {
        qWarning("File not found: %s, available files: %s",
                 qUtf8Printable(fileId),
                 qUtf8Printable(QStringList(m_localFiles.keys()).join(", ")));
        socket->write("ERROR|not_found\n");
        socket->flush();
        socket->disconnectFromHost();
        return;
    }

    SharedFileInfo fileInfo = m_localFiles[fileId];
    QFile* file = new QFile(fileInfo.filePath, this);

    if (!file->open(QIODevice::ReadOnly)) {
        qWarning("Failed to open file: %s", qUtf8Printable(fileInfo.filePath));
        socket->write("ERROR|file_open_failed\n");
        socket->flush();
        socket->disconnectFromHost();
        file->deleteLater();
        return;
    }

    // 发送响应头: OK|fileSize\n
    QByteArray header = QStringLiteral("OK|%1\n").arg(file->size()).toUtf8();
    socket->write(header);

    // 设置上传传输信息
    TransferInfo info;
    info.fileId = fileId;
    info.fileName = fileInfo.fileName;
    info.filePath = fileInfo.filePath;
    info.fileSize = file->size();
    info.bytesTransferred = 0;
    info.lastProgressBytes = 0;
    info.lastProgressAtMs = 0;
    info.isUpload = true;
    info.socket = socket;

    m_activeTransfers[socket] = info;

    // 存储文件句柄供后续使用
    socket->setProperty("file", QVariant::fromValue(file));
    socket->setProperty("upload_finished", false);
    m_pendingUploadChunks[socket] = QByteArray();

    emit uploadStarted(fileId);

    // 连接 bytesWritten 一次，持续发送数据直到完成
    connect(socket, &QTcpSocket::bytesWritten, this, [this, socket]() {
        sendFileChunk(socket);
    });

    // 开始发送文件数据
    sendFileChunk(socket);
}

void FileTransferManager::sendFileChunk(QTcpSocket* socket) {
    if (!m_activeTransfers.contains(socket)) {
        return;
    }
    if (socket->property("upload_finished").toBool()) {
        return;
    }

    TransferInfo& info = m_activeTransfers[socket];
    QFile* file = socket->property("file").value<QFile*>();

    if (!file || !file->isOpen()) {
        qWarning("File handle is invalid");
        socket->disconnectFromHost();
        return;
    }

    while (socket->bytesToWrite() < kSocketBufferSize) {
        QByteArray& pendingChunk = m_pendingUploadChunks[socket];
        if (!pendingChunk.isEmpty()) {
            const qint64 pendingWritten = socket->write(pendingChunk);
            if (pendingWritten <= 0) {
                return;
            }
            info.bytesTransferred += pendingWritten;
            if (pendingWritten < pendingChunk.size()) {
                pendingChunk.remove(0, pendingWritten);
            } else {
                pendingChunk.clear();
            }
            if (shouldReportProgress(info)) {
                emit uploadProgress(info.fileId, info.bytesTransferred, info.fileSize);
            }
            continue;
        }

        // 读取并发送一块数据
        QByteArray chunk = file->read(Constants::TRANSFER_CHUNK_SIZE);
        if (chunk.isEmpty()) {
            if (file->atEnd()) {
                socket->setProperty("upload_finished", true);
                emit uploadComplete(info.fileId);
                socket->flush();
                socket->disconnectFromHost();
            } else {
                qWarning("Failed to read source file while uploading");
                socket->disconnectFromHost();
            }
            return;
        }

        const qint64 written = socket->write(chunk);
        if (written <= 0) {
            return;
        }
        if (written < chunk.size()) {
            pendingChunk = chunk.mid(written);
        }

        info.bytesTransferred += written;
        if (shouldReportProgress(info)) {
            emit uploadProgress(info.fileId, info.bytesTransferred, info.fileSize);
        }
    }
}

void FileTransferManager::handleDownloadResponse(QTcpSocket* socket) {
    if (!m_activeTransfers.contains(socket)) {
        return;
    }

    TransferInfo& info = m_activeTransfers[socket];
    auto reportDownloadError = [this, socket, &info](const QString& error) {
        socket->setProperty("download_error_reported", true);
        emit downloadError(info.fileId, error);
        socket->disconnectFromHost();
    };

    QByteArray data = socket->readAll();
    m_buffers[socket].append(data);

    // 如果还没有接收文件大小信息
    if (info.fileSize == 0) {
        int newlineIndex = m_buffers[socket].indexOf('\n');
        if (newlineIndex == -1) {
            return;  // 等待响应头
        }

        QByteArray header = m_buffers[socket].left(newlineIndex);
        m_buffers[socket] = m_buffers[socket].mid(newlineIndex + 1);

        QString headerStr = QString::fromUtf8(header).trimmed();
        QStringList parts = headerStr.split('|');

        if (parts.size() < 2) {
            reportDownloadError(QStringLiteral("Invalid response header"));
            return;
        }

        QString status = parts[0];
        if (status == QStringLiteral("ERROR")) {
            QString error = parts.size() > 1 ? parts[1] : QStringLiteral("Unknown error");
            reportDownloadError(error);
            return;
        }

        if (status != QStringLiteral("OK")) {
            reportDownloadError(QStringLiteral("Unexpected response"));
            return;
        }

        info.fileSize = parts[1].toLongLong();
        if (info.fileSize <= 0) {
            reportDownloadError(QStringLiteral("Invalid file size"));
            return;
        }

        // 打开文件准备写入
        QDir dir;
        QString dirPath = QFileInfo(info.filePath).absolutePath();
        if (!dir.exists(dirPath)) {
            dir.mkpath(dirPath);
        }

        QFile* file = new QFile(info.filePath, this);
        if (!file->open(QIODevice::WriteOnly)) {
            reportDownloadError(QStringLiteral("Failed to create local file"));
            file->deleteLater();
            return;
        }
        if (!file->resize(info.fileSize)) {
            qWarning() << "Failed to pre-allocate target file:" << info.filePath;
        }
        if (!file->seek(0)) {
            reportDownloadError(QStringLiteral("Failed to initialize local file"));
            file->deleteLater();
            return;
        }

        socket->setProperty("file", QVariant::fromValue(file));
    }

    // 写入接收到的数据
    QFile* file = socket->property("file").value<QFile*>();
    if (file && file->isOpen()) {
        auto writeAll = [file](const char* data, qint64 size) -> bool {
            qint64 totalWritten = 0;
            while (totalWritten < size) {
                const qint64 written = file->write(data + totalWritten, size - totalWritten);
                if (written <= 0) {
                    return false;
                }
                totalWritten += written;
            }
            return true;
        };

        QByteArray& bufferedData = m_buffers[socket];
        if (!bufferedData.isEmpty()) {
            if (!writeAll(bufferedData.constData(), bufferedData.size())) {
                reportDownloadError(QStringLiteral("Failed to write local file"));
                return;
            }
            info.bytesTransferred += bufferedData.size();
            bufferedData.clear();
        }

        std::array<char, static_cast<size_t>(kDownloadIoBufferSize)> ioBuffer{};
        while (socket->bytesAvailable() > 0) {
            const qint64 readSize = qMin<qint64>(socket->bytesAvailable(), kDownloadIoBufferSize);
            const qint64 read = socket->read(ioBuffer.data(), readSize);
            if (read <= 0) {
                break;
            }
            if (!writeAll(ioBuffer.data(), read)) {
                reportDownloadError(QStringLiteral("Failed to write local file"));
                return;
            }
            info.bytesTransferred += read;
        }

        if (shouldReportProgress(info)) {
            emit downloadProgress(info.fileId, info.bytesTransferred, info.fileSize);
        }

        // 检查是否完成
        if (info.bytesTransferred >= info.fileSize) {
            file->close();
            if (info.lastProgressBytes < info.bytesTransferred) {
                emit downloadProgress(info.fileId, info.bytesTransferred, info.fileSize);
            }
            emit downloadComplete(info.fileId, info.filePath);
            socket->disconnectFromHost();
        }
    }
}

void FileTransferManager::handleSocketError(QTcpSocket* socket) {
    if (!socket) return;

    if (!m_activeTransfers.contains(socket)) {
        return;
    }

    TransferInfo& info = m_activeTransfers[socket];

    qDebug() << "Socket error: isUpload:" << info.isUpload
             << "fileId:" << info.fileId
             << "error:" << socket->errorString();

    const bool alreadyReported = socket->property("download_error_reported").toBool();
    const bool isNormalRemoteClose =
        socket->error() == QAbstractSocket::RemoteHostClosedError
        && info.fileSize > 0
        && info.bytesTransferred >= info.fileSize;

    if (!info.isUpload && !alreadyReported && !isNormalRemoteClose) {
        // 删除部分下载的文件
        if (!info.filePath.isEmpty()) {
            QFile::remove(info.filePath);
        }
        emit downloadError(info.fileId, socket->errorString());
    }

    cleanupTransfer(socket);
}

void FileTransferManager::cleanupTransfer(QTcpSocket* socket) {
    if (!socket) {
        return;
    }
    if (socket->property("transfer_cleaned").toBool()) {
        return;
    }
    socket->setProperty("transfer_cleaned", true);

    m_activeTransfers.remove(socket);
    m_buffers.remove(socket);
    m_pendingUploadChunks.remove(socket);

    QFile* file = socket->property("file").value<QFile*>();
    if (file) {
        if (file->isOpen()) {
            file->close();
        }
        file->deleteLater();
        socket->setProperty("file", QVariant());
    }

    socket->deleteLater();
}
