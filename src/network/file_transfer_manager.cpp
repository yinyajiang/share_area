#include "file_transfer_manager.h"
#include <QTcpSocket>
#include <QFile>
#include <QDir>

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
    // 关闭所有活跃传输
    for (auto it = m_activeTransfers.begin(); it != m_activeTransfers.end(); ++it) {
        QTcpSocket* socket = it.key();
        socket->disconnectFromHost();
        socket->deleteLater();
    }
    m_activeTransfers.clear();
    m_buffers.clear();

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
    QTcpSocket* socket = new QTcpSocket(this);

    // 设置传输信息
    TransferInfo info;
    info.fileId = fileInfo.fileId;
    info.fileName = fileInfo.fileName;
    info.filePath = savePath;
    info.fileSize = fileInfo.fileSize;
    info.bytesTransferred = 0;
    info.isUpload = false;
    info.socket = socket;

    m_activeTransfers[socket] = info;
    m_buffers[socket] = QByteArray();

    // 连接信号
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        if (m_activeTransfers[socket].isUpload) {
            handleClientRequest(socket);
        } else {
            handleDownloadResponse(socket);
        }
    });
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, [this, socket](QAbstractSocket::SocketError error) {
        onSocketError(error);
    });

    // 连接成功后发送文件请求
    connect(socket, &QTcpSocket::connected, this, [this, socket]() {
        QByteArray request = QStringLiteral("GET|%1\n").arg(
            m_activeTransfers[socket].fileId).toUtf8();
        socket->write(request);
    });

    // 连接到远程服务器
    socket->connectToHost(peerAddress, peerPort);
}

void FileTransferManager::onNewConnection() {
    QTcpSocket* clientSocket = m_server->nextPendingConnection();

    // 标记为上传连接，使 readyRead 路由到 handleClientRequest
    TransferInfo info;
    info.isUpload = true;
    info.socket = clientSocket;
    m_activeTransfers[clientSocket] = info;

    connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]() {
        if (m_activeTransfers[clientSocket].isUpload) {
            handleClientRequest(clientSocket);
        } else {
            handleDownloadResponse(clientSocket);
        }
    });
    connect(clientSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, [this, clientSocket](QAbstractSocket::SocketError error) {
        onSocketError(error);
    });

    // 初始化空缓冲区
    m_buffers[clientSocket] = QByteArray();
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
        qWarning("File not found: %s", qUtf8Printable(fileId));
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
    info.isUpload = true;
    info.socket = socket;

    m_activeTransfers[socket] = info;

    // 存储文件句柄供后续使用
    socket->setProperty("file", QVariant::fromValue(file));

    emit uploadStarted(fileId);

    // 开始发送文件数据
    sendFileChunk(socket);
}

void FileTransferManager::sendFileChunk(QTcpSocket* socket) {
    if (!m_activeTransfers.contains(socket)) {
        return;
    }

    TransferInfo& info = m_activeTransfers[socket];
    QFile* file = socket->property("file").value<QFile*>();

    if (!file || !file->isOpen()) {
        qWarning("File handle is invalid");
        socket->disconnectFromHost();
        return;
    }

    // 读取并发送一块数据
    QByteArray chunk = file->read(Constants::TRANSFER_CHUNK_SIZE);
    qint64 written = socket->write(chunk);

    if (written > 0) {
        info.bytesTransferred += written;
        emit uploadProgress(info.fileId, info.bytesTransferred, info.fileSize);

        // 如果还有数据，继续发送
        if (!file->atEnd()) {
            connect(socket, &QTcpSocket::bytesWritten, this, [this, socket](qint64) {
                sendFileChunk(socket);
            });
        } else {
            // 传输完成
            emit uploadComplete(info.fileId);
            socket->flush();
            socket->disconnectFromHost();
        }
    }
}

void FileTransferManager::handleDownloadResponse(QTcpSocket* socket) {
    if (!m_activeTransfers.contains(socket)) {
        return;
    }

    TransferInfo& info = m_activeTransfers[socket];
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
            emit downloadError(info.fileId, QStringLiteral("Invalid response header"));
            socket->disconnectFromHost();
            return;
        }

        QString status = parts[0];
        if (status == QStringLiteral("ERROR")) {
            QString error = parts.size() > 1 ? parts[1] : QStringLiteral("Unknown error");
            emit downloadError(info.fileId, error);
            socket->disconnectFromHost();
            return;
        }

        if (status != QStringLiteral("OK")) {
            emit downloadError(info.fileId, QStringLiteral("Unexpected response"));
            socket->disconnectFromHost();
            return;
        }

        info.fileSize = parts[1].toLongLong();

        // 打开文件准备写入
        QDir dir;
        QString dirPath = QFileInfo(info.filePath).absolutePath();
        if (!dir.exists(dirPath)) {
            dir.mkpath(dirPath);
        }

        QFile* file = new QFile(info.filePath, this);
        if (!file->open(QIODevice::WriteOnly)) {
            emit downloadError(info.fileId, QStringLiteral("Failed to create local file"));
            socket->disconnectFromHost();
            file->deleteLater();
            return;
        }

        socket->setProperty("file", QVariant::fromValue(file));
    }

    // 写入接收到的数据
    QFile* file = socket->property("file").value<QFile*>();
    if (file && file->isOpen()) {
        file->write(m_buffers[socket]);
        info.bytesTransferred += m_buffers[socket].size();
        m_buffers[socket].clear();

        emit downloadProgress(info.fileId, info.bytesTransferred, info.fileSize);

        // 检查是否完成
        if (info.bytesTransferred >= info.fileSize) {
            file->close();
            emit downloadComplete(info.fileId, info.filePath);
            socket->disconnectFromHost();
        }
    }
}

void FileTransferManager::onSocketError(QAbstractSocket::SocketError error) {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    if (!m_activeTransfers.contains(socket)) {
        return;
    }

    TransferInfo& info = m_activeTransfers[socket];

    if (!info.isUpload) {
        emit downloadError(info.fileId, socket->errorString());
    }

    // 清理资源
    QFile* file = socket->property("file").value<QFile*>();
    if (file) {
        file->close();
        file->deleteLater();
    }

    m_activeTransfers.remove(socket);
    m_buffers.remove(socket);
    socket->deleteLater();
}
