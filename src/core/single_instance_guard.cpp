#include "single_instance_guard.h"

#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>

namespace {

QString lockFilePathForServerName(const QString &serverName) {
    QString safeName = serverName;
    for (qsizetype i = 0; i < safeName.size(); ++i) {
        const QChar ch = safeName.at(i);
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('_') &&
            ch != QLatin1Char('-') && ch != QLatin1Char('.')) {
            safeName[i] = QLatin1Char('_');
        }
    }
    return QDir::temp().absoluteFilePath(QStringLiteral("ShareArea-") +
                                         safeName + QStringLiteral(".lock"));
}

} // namespace

SingleInstanceGuard::SingleInstanceGuard(const QString &serverName,
                                         QObject *parent)
    : QObject(parent), m_serverName(serverName),
      m_lockFilePath(lockFilePathForServerName(serverName)) {}

SingleInstanceGuard::~SingleInstanceGuard() {
    if (m_server) {
        m_server->close();
    }
    if (m_acquired) {
        QLocalServer::removeServer(m_serverName);
    }
    if (m_lockFile && m_lockFile->isLocked()) {
        m_lockFile->unlock();
    }
}

bool SingleInstanceGuard::tryAcquire() {
    if (m_acquired) {
        return true;
    }

    if (!m_lockFile) {
        m_lockFile = std::make_unique<QLockFile>(m_lockFilePath);
    }

    if (!m_lockFile->tryLock(0)) {
        m_errorString = QStringLiteral("Another instance is already running.");
        return false;
    }

    // Only the process holding the lock may clean up a stale local server name.
    QLocalServer::removeServer(m_serverName);

    if (!listen()) {
        if (m_lockFile->isLocked()) {
            m_lockFile->unlock();
        }
        m_lockFile.reset();
        return false;
    }

    return true;
}

bool SingleInstanceGuard::notifyPrimaryInstance(const QByteArray &message,
                                                int timeoutMs) const {
    QLocalSocket socket;
    socket.connectToServer(m_serverName);
    if (!socket.waitForConnected(timeoutMs)) {
        return false;
    }

    socket.write(message);
    if (!socket.waitForBytesWritten(timeoutMs)) {
        return false;
    }

    socket.disconnectFromServer();
    return true;
}

QString SingleInstanceGuard::errorString() const { return m_errorString; }

bool SingleInstanceGuard::listen() {
    if (!m_server) {
        m_server = new QLocalServer(this);
        connect(m_server, &QLocalServer::newConnection, this,
                &SingleInstanceGuard::handleNewConnection);
    }

    if (!m_server->listen(m_serverName)) {
        m_errorString = m_server->errorString();
        return false;
    }

    m_acquired = true;
    m_errorString.clear();
    return true;
}

void SingleInstanceGuard::handleNewConnection() {
    while (m_server && m_server->hasPendingConnections()) {
        QLocalSocket *socket = m_server->nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            const QByteArray message = socket->readAll().trimmed();
            if (message.isEmpty() || message == QByteArrayLiteral("show")) {
                emit showRequested();
            }
        });
        connect(socket, &QLocalSocket::disconnected, socket,
                &QLocalSocket::deleteLater);
    }
}
