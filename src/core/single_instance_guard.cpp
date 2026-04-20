#include "single_instance_guard.h"

#include <QLocalServer>
#include <QLocalSocket>

SingleInstanceGuard::SingleInstanceGuard(const QString &serverName,
                                         QObject *parent)
    : QObject(parent), m_serverName(serverName) {}

SingleInstanceGuard::~SingleInstanceGuard() {
    if (m_server) {
        m_server->close();
    }
    if (m_acquired) {
        QLocalServer::removeServer(m_serverName);
    }
}

bool SingleInstanceGuard::tryAcquire() {
    if (m_acquired) {
        return true;
    }

    if (listen()) {
        return true;
    }

    QLocalSocket probe;
    probe.connectToServer(m_serverName, QIODevice::WriteOnly);
    if (probe.waitForConnected(100)) {
        probe.disconnectFromServer();
        m_errorString = QStringLiteral("Another instance is already running.");
        return false;
    }

    QLocalServer::removeServer(m_serverName);
    if (m_server) {
        delete m_server;
        m_server = nullptr;
    }

    return listen();
}

bool SingleInstanceGuard::notifyPrimaryInstance(const QByteArray &message,
                                                int timeoutMs) const {
    QLocalSocket socket;
    socket.connectToServer(m_serverName, QIODevice::WriteOnly);
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
