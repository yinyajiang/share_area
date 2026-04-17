#include "file_transfer_manager.h"
#include <QTcpSocket>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QNetworkProxy>
#include <QDateTime>
#include <QTimer>
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
                if (!info.isUpload && info.bytesTransferred > 0) {
                    if (info.isDirectory && !info.folderRootPath.isEmpty()) {
                        // 删除部分下载的文件夹
                        QDir(info.folderRootPath).removeRecursively();
                    } else if (info.bytesTransferred < info.fileSize && !info.filePath.isEmpty()) {
                        QFile::remove(info.filePath);
                    }
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
    info.isDirectory = fileInfo.isDirectory();
    info.fileSize = 0;  // 由远端响应头提供
    info.bytesTransferred = 0;
    info.isUpload = false;
    info.shareType = fileInfo.shareType;
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
            // 删除部分下载的文件或文件夹
            if (info.isDirectory && !info.folderRootPath.isEmpty()) {
                QDir(info.folderRootPath).removeRecursively();
            } else if (!info.filePath.isEmpty()) {
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

QStringList FileTransferManager::enumerateDirFiles(const QString& rootPath) {
    QStringList files;
    QDirIterator it(rootPath, QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    QString rootPathCanonical = QDir(rootPath).canonicalPath();
    if (!rootPathCanonical.endsWith('/')) {
        rootPathCanonical += '/';
    }

    while (it.hasNext()) {
        it.next();
        QFileInfo fi(it.fileInfo());
        // Skip symlinks to avoid infinite loops
        if (fi.isSymLink()) continue;
        // Get relative path from root
        QString fullPath = QDir(fi.absoluteFilePath()).canonicalPath();
        QString relativePath = fullPath;
        if (fullPath.startsWith(rootPathCanonical)) {
            relativePath = fullPath.mid(rootPathCanonical.length());
        }
        if (!relativePath.isEmpty()) {
            files.append(relativePath);
        }
    }
    return files;
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

    if (fileInfo.isDirectory()) {
        handleDirClientRequest(socket, fileId);
        return;
    }

    // 所有剪贴板类型走内存传输（不使用临时文件）
    if (fileInfo.isClipboard()) {
        handleClipboardClientRequest(socket, fileId);
        return;
    }

    // Regular file upload
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

void FileTransferManager::handleClipboardClientRequest(QTcpSocket* socket, const QString& fileId) {
    SharedFileInfo fileInfo = m_localFiles[fileId];
    QByteArray content = fileInfo.rawData;  // text: UTF-8 bytes; image: PNG bytes
    qint64 contentSize = content.size();

    qDebug() << "Clipboard upload requested:" << fileInfo.fileName << "size:" << contentSize
             << "type:" << static_cast<int>(fileInfo.shareType);

    // 发送响应头: OK|fileSize\n
    QByteArray header = QStringLiteral("OK|%1\n").arg(contentSize).toUtf8();
    socket->write(header);

    // 直接发送全部内容（剪贴板文本不会太大）
    qint64 written = 0;
    while (written < contentSize) {
        qint64 w = socket->write(content.constData() + written, contentSize - written);
        if (w <= 0) {
            qWarning("Failed to write clipboard data to socket");
            socket->disconnectFromHost();
            return;
        }
        written += w;
    }
    socket->flush();

    // 设置上传传输信息（用于进度和完成通知）
    TransferInfo info;
    info.fileId = fileId;
    info.fileName = fileInfo.fileName;
    info.fileSize = contentSize;
    info.bytesTransferred = contentSize;
    info.lastProgressBytes = 0;
    info.lastProgressAtMs = 0;
    info.isUpload = true;
    info.socket = socket;
    m_activeTransfers[socket] = info;

    emit uploadStarted(fileId);
    emit uploadComplete(fileId);

    // 短暂延迟后关闭连接，确保数据发送
    QTimer::singleShot(100, socket, &QTcpSocket::disconnectFromHost);
}

void FileTransferManager::handleDirClientRequest(QTcpSocket* socket, const QString& fileId) {
    SharedFileInfo fileInfo = m_localFiles[fileId];
    QString rootPath = fileInfo.filePath;

    // 枚举所有文件
    QStringList relativePaths = enumerateDirFiles(rootPath);
    qint64 totalSize = fileInfo.fileSize;
    int fileCount = relativePaths.size();

    qDebug() << "Directory upload requested:" << fileInfo.fileName
             << "files:" << fileCount << "totalSize:" << totalSize;

    // 发送目录响应头: OK_DIR|fileCount|totalSize\n
    QByteArray header = QStringLiteral("OK_DIR|%1|%2\n").arg(fileCount).arg(totalSize).toUtf8();
    socket->write(header);
    socket->flush();  // 确保响应头被发送

    // 设置上传传输信息
    TransferInfo info;
    info.fileId = fileId;
    info.fileName = fileInfo.fileName;
    info.filePath = rootPath;
    info.fileSize = totalSize;
    info.totalFiles = fileCount;
    info.completedFiles = 0;
    info.isDirectory = true;
    info.pendingRelativePaths = relativePaths;
    info.folderRootPath = rootPath;
    info.bytesTransferred = 0;
    info.lastProgressBytes = 0;
    info.lastProgressAtMs = 0;
    info.isUpload = true;
    info.socket = socket;

    m_activeTransfers[socket] = info;
    socket->setProperty("upload_finished", false);
    m_pendingUploadChunks[socket] = QByteArray();

    emit uploadStarted(fileId);

    // 连接 bytesWritten 持续发送数据
    connect(socket, &QTcpSocket::bytesWritten, this, [this, socket]() {
        sendDirFileChunk(socket);
    });

    // 开始发送文件数据
    sendDirFileChunk(socket);
}

void FileTransferManager::sendDirFileChunk(QTcpSocket* socket) {
    if (!m_activeTransfers.contains(socket)) {
        return;
    }
    if (socket->property("upload_finished").toBool()) {
        return;
    }

    TransferInfo& info = m_activeTransfers[socket];

    while (socket->bytesToWrite() < kSocketBufferSize) {
        // 首先检查是否有待发送的块
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
            // 继续发送更多数据
            continue;
        }

        QFile* currentFile = socket->property("file").value<QFile*>();
        if (currentFile && currentFile->isOpen()) {
            // 正在发送当前文件的数据
            QByteArray chunk = currentFile->read(Constants::TRANSFER_CHUNK_SIZE);
            if (chunk.isEmpty()) {
                if (currentFile->atEnd()) {
                    // 当前文件完成
                    currentFile->close();
                    currentFile->deleteLater();
                    socket->setProperty("file", QVariant());
                    info.completedFiles++;
                    qDebug() << "Directory file" << info.completedFiles << "/" << info.totalFiles << "completed";
                    // 文件完成，继续循环以开始下一个文件
                    continue;
                } else {
                    qWarning("Failed to read source file while uploading directory");
                    socket->disconnectFromHost();
                    return;
                }
            }

            // 发送数据块
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
            continue;
        }

        // 没有打开的文件，检查是否所有文件都已完成
        if (info.completedFiles >= info.totalFiles) {
            // 所有文件完成，检查是否所有数据都已发送
            if (socket->bytesToWrite() == 0) {
                // 所有数据都已发送完成
                qDebug() << "Directory upload complete, all files sent, disconnecting";
                socket->setProperty("upload_finished", true);
                emit uploadComplete(info.fileId);
                socket->disconnectFromHost();
                return;
            }
            // 还有数据在发送中，等待 bytesWritten 信号再次触发
            qDebug() << "All files completed but " << socket->bytesToWrite() << " bytes still in write buffer, waiting...";
            return;
        }

        // 需要开始下一个文件
        if (info.completedFiles < info.pendingRelativePaths.size()) {
            QString relativePath = info.pendingRelativePaths[info.completedFiles];
            QString fullPath = QDir(info.folderRootPath).filePath(relativePath);
            qint64 fileSize = QFileInfo(fullPath).size();

            qDebug() << "Sending file:" << relativePath << "size:" << fileSize;

            // 发送文件头: FILE|relativePath|fileSize\n
            QByteArray fileHeader = QStringLiteral("FILE|%1|%2\n")
                .arg(relativePath)
                .arg(fileSize)
                .toUtf8();
            socket->write(fileHeader);
            socket->flush();  // 确保文件头被发送

            // 打开文件准备读取
            QFile* file = new QFile(fullPath, this);
            if (!file->open(QIODevice::ReadOnly)) {
                qWarning("Failed to open file in directory: %s", qUtf8Printable(fullPath));
                socket->disconnectFromHost();
                file->deleteLater();
                return;
            }
            socket->setProperty("file", QVariant::fromValue(file));
            // 文件已打开，下次循环开始发送数据
        }
    }
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

        if (status == QStringLiteral("OK_DIR")) {
            // Directory response: OK_DIR|fileCount|totalSize
            if (parts.size() < 3) {
                reportDownloadError(QStringLiteral("Invalid directory response header"));
                return;
            }
            info.isDirectory = true;
            info.totalFiles = parts[1].toInt();
            info.fileSize = parts[2].toLongLong();
            info.completedFiles = 0;

            qDebug() << "Directory download: files=" << info.totalFiles << "totalSize=" << info.fileSize;

            // 创建目标文件夹
            QDir dir;
            if (!dir.exists(info.filePath)) {
                dir.mkpath(info.filePath);
            }
            info.folderRootPath = info.filePath;

            // 开始接收第一个文件
            if (info.totalFiles > 0) {
                // 等待下一个 FILE 响应头
                return;
            } else {
                // 空文件夹，直接完成
                emit downloadComplete(info.fileId, info.filePath);
                socket->disconnectFromHost();
            }
            return;
        }

        if (status != QStringLiteral("OK")) {
            reportDownloadError(QStringLiteral("Unexpected response"));
            return;
        }

        // Regular file response: OK|fileSize
        info.fileSize = parts[1].toLongLong();
        if (info.fileSize <= 0) {
            reportDownloadError(QStringLiteral("Invalid file size"));
            return;
        }

        // 剪贴板下载：filePath 为空，纯内存接收
        if (info.filePath.isEmpty()) {
            // Data will be accumulated in memoryBuffer
            qDebug() << "Clipboard download: in-memory, size:" << info.fileSize
                     << "type:" << static_cast<int>(info.shareType);
        } else {
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
        }  // end else (non-clipboard file download)
    }

    // 文件夹下载 - 处理每个文件
    if (info.isDirectory) {
        handleDirDownloadResponse(socket);
        return;
    }

    // 剪贴板下载：纯内存接收，不写文件
    if (info.filePath.isEmpty()) {
        QByteArray& bufferedData = m_buffers[socket];
        if (!bufferedData.isEmpty()) {
            info.memoryBuffer.append(bufferedData);
            info.bytesTransferred += bufferedData.size();
            bufferedData.clear();
        }
        while (socket->bytesAvailable() > 0) {
            QByteArray chunk = socket->read(kDownloadIoBufferSize);
            if (chunk.isEmpty()) break;
            info.memoryBuffer.append(chunk);
            info.bytesTransferred += chunk.size();
        }
        if (shouldReportProgress(info)) {
            emit downloadProgress(info.fileId, info.bytesTransferred, info.fileSize);
        }
        if (info.bytesTransferred >= info.fileSize) {
            if (info.lastProgressBytes < info.bytesTransferred) {
                emit downloadProgress(info.fileId, info.bytesTransferred, info.fileSize);
            }
            emit clipboardReceived(info.fileId, info.memoryBuffer,
                                   static_cast<int>(info.shareType));
            socket->disconnectFromHost();
        }
        return;
    }

    // 写入接收到的数据（单文件）
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

void FileTransferManager::handleDirDownloadResponse(QTcpSocket* socket) {
    if (!m_activeTransfers.contains(socket)) {
        return;
    }

    TransferInfo& info = m_activeTransfers[socket];
    auto reportDownloadError = [this, socket, &info](const QString& error) {
        socket->setProperty("download_error_reported", true);
        emit downloadError(info.fileId, error);
        socket->disconnectFromHost();
    };

    // 检查是否需要读取文件头
    if (info.currentFileSize == 0) {
        int newlineIndex = m_buffers[socket].indexOf('\n');
        if (newlineIndex == -1) {
            qDebug() << "Directory download: waiting for FILE header, buffer size:" << m_buffers[socket].size();
            return;  // 等待文件头
        }

        QByteArray header = m_buffers[socket].left(newlineIndex);
        m_buffers[socket] = m_buffers[socket].mid(newlineIndex + 1);

        QString headerStr = QString::fromUtf8(header).trimmed();
        QStringList parts = headerStr.split('|');

        if (parts.size() < 3 || parts[0] != QStringLiteral("FILE")) {
            reportDownloadError(QStringLiteral("Expected FILE header in directory transfer"));
            return;
        }

        info.currentRelativePath = parts[1];
        info.currentFileSize = parts[2].toLongLong();
        info.currentFileTransferred = 0;

        qDebug() << "Receiving file:" << info.currentRelativePath << "size:" << info.currentFileSize;

        // 创建子目录并打开文件
        QString fullPath = QDir(info.folderRootPath).filePath(info.currentRelativePath);
        QFileInfo fullFileInfo(fullPath);
        QDir().mkpath(fullFileInfo.absolutePath());

        QFile* file = new QFile(fullPath, this);
        if (!file->open(QIODevice::WriteOnly)) {
            reportDownloadError(QStringLiteral("Failed to create file: ") + fullPath);
            file->deleteLater();
            return;
        }
        if (!file->resize(info.currentFileSize)) {
            qWarning() << "Failed to pre-allocate file:" << fullPath;
        }
        if (!file->seek(0)) {
            reportDownloadError(QStringLiteral("Failed to seek file: ") + fullPath);
            file->deleteLater();
            return;
        }

        socket->setProperty("file", QVariant::fromValue(file));
    }

    // 写入当前文件的数据
    QFile* file = socket->property("file").value<QFile*>();
    if (!file || !file->isOpen()) {
        reportDownloadError(QStringLiteral("File handle invalid during directory download"));
        return;
    }

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
    qint64 bytesToWrite = bufferedData.size() + socket->bytesAvailable();
    qint64 bytesRemainingInFile = info.currentFileSize - info.currentFileTransferred;
    qint64 bytesToWriteThisFile = qMin(bytesToWrite, bytesRemainingInFile);

    qint64 written = 0;
    while (written < bytesToWriteThisFile) {
        qint64 chunkSize = qMin(qMin<qint64>(kDownloadIoBufferSize, bytesToWriteThisFile - written),
                                socket->bytesAvailable() + bufferedData.size());

        if (bufferedData.isEmpty()) {
            // 直接从 socket 读取
            std::array<char, static_cast<size_t>(kDownloadIoBufferSize)> ioBuffer{};
            qint64 toRead = qMin(chunkSize, socket->bytesAvailable());
            qint64 read = socket->read(ioBuffer.data(), toRead);
            if (read <= 0) break;
            if (!writeAll(ioBuffer.data(), read)) {
                reportDownloadError(QStringLiteral("Failed to write file during directory download"));
                return;
            }
            written += read;
        } else {
            // 写入缓冲数据
            qint64 toWrite = qMin(qMin<qint64>(bufferedData.size(), bytesToWriteThisFile - written), chunkSize);
            if (!writeAll(bufferedData.constData(), toWrite)) {
                reportDownloadError(QStringLiteral("Failed to write file during directory download"));
                return;
            }
            bufferedData.remove(0, toWrite);
            written += toWrite;
        }
    }

    info.currentFileTransferred += bytesToWriteThisFile;
    info.bytesTransferred += bytesToWriteThisFile;

    if (shouldReportProgress(info)) {
        emit downloadProgress(info.fileId, info.bytesTransferred, info.fileSize);
    }

    // 检查当前文件是否完成
    if (info.currentFileTransferred >= info.currentFileSize) {
        file->close();
        file->deleteLater();
        socket->setProperty("file", QVariant());

        info.completedFiles++;
        qDebug() << "File completed:" << info.completedFiles << "/" << info.totalFiles;

        // 重置状态准备接收下一个文件
        info.currentFileSize = 0;
        info.currentFileTransferred = 0;
        info.currentRelativePath.clear();

        // 检查是否所有文件都完成
        if (info.completedFiles >= info.totalFiles) {
            if (info.lastProgressBytes < info.bytesTransferred) {
                emit downloadProgress(info.fileId, info.bytesTransferred, info.fileSize);
            }
            emit downloadComplete(info.fileId, info.folderRootPath);
            // 不主动断开连接，等待发送方断开
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
             << "isDirectory:" << info.isDirectory
             << "fileSize:" << info.fileSize
             << "bytesTransferred:" << info.bytesTransferred
             << "completedFiles:" << info.completedFiles
             << "/" << info.totalFiles
             << "error:" << socket->errorString();

    const bool alreadyReported = socket->property("download_error_reported").toBool();
    const bool isNormalRemoteClose =
        socket->error() == QAbstractSocket::RemoteHostClosedError
        && info.fileSize > 0
        && info.bytesTransferred >= info.fileSize;

    if (!info.isUpload && !alreadyReported && !isNormalRemoteClose) {
        // 删除部分下载的文件或文件夹
        if (info.isDirectory && !info.folderRootPath.isEmpty()) {
            qDebug() << "Cleaning up incomplete directory download:" << info.folderRootPath;
            QDir(info.folderRootPath).removeRecursively();
        } else if (!info.filePath.isEmpty()) {
            qDebug() << "Cleaning up incomplete file download:" << info.filePath;
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
