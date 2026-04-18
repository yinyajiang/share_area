#include "peer_discovery.h"
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include "core/app_settings.h"

PeerDiscovery::PeerDiscovery(const QString& groupCode, QObject* parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_broadcastTimer(nullptr)
    , m_timeoutTimer(nullptr)
    , m_groupCode(groupCode)
    , m_transferPort(0)
{
    m_socket = new QUdpSocket(this);
    m_broadcastTimer = new QTimer(this);
    m_timeoutTimer = new QTimer(this);

    m_deviceId = AppSettings::instance().deviceId();

    connect(m_broadcastTimer, &QTimer::timeout, this, &PeerDiscovery::sendAnnouncement);
    connect(m_timeoutTimer, &QTimer::timeout, this, &PeerDiscovery::checkPeerTimeouts);
    connect(m_socket, &QUdpSocket::readyRead, this, &PeerDiscovery::onReadyRead);
}

PeerDiscovery::~PeerDiscovery() {
    stop();
}

void PeerDiscovery::setupSocket() {
    if (m_socket->state() == QUdpSocket::UnconnectedState) {
        if (!m_socket->bind(QHostAddress::AnyIPv4, Constants::DISCOVERY_PORT,
                           QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            qWarning("Failed to bind UDP socket on port %d", Constants::DISCOVERY_PORT);
        }
    }
}

void PeerDiscovery::start() {
    setupSocket();

    m_broadcastTimer->start(Constants::BROADCAST_INTERVAL_MS);
    m_timeoutTimer->start(2000);

    sendAnnouncement();
}

void PeerDiscovery::stop() {
    m_broadcastTimer->stop();
    m_timeoutTimer->stop();

    QByteArray message = QStringLiteral("GOODBYE|%1|%2|%3")
        .arg(m_groupCode)
        .arg(m_deviceId)
        .arg(m_deviceName)
        .toUtf8();
    sendMessage(message);

    m_socket->close();
    m_peers.clear();
}

void PeerDiscovery::setTransferPort(int port) {
    m_transferPort = port;
}

void PeerDiscovery::setMultiAddressBroadcast(bool on) {
    m_multiAddressBroadcast = on;
}

QList<QHostAddress> PeerDiscovery::broadcastAddresses() const {
    QList<QHostAddress> addresses;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
        if (!(iface.flags() & QNetworkInterface::IsUp)) continue;
        if (!(iface.flags() & QNetworkInterface::IsRunning)) continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            QHostAddress broadcast = entry.broadcast();
            if (!broadcast.isNull()) {
                addresses.append(broadcast);
            }
        }
    }
    return addresses;
}

void PeerDiscovery::sendAnnouncement() {
    if (m_deviceName.isEmpty()) {
        m_deviceName = AppSettings::instance().deviceName();
    }

    QByteArray message = QStringLiteral("ANNOUNCE|%1|%2|%3|%4")
        .arg(m_groupCode)
        .arg(m_deviceId)
        .arg(m_deviceName)
        .arg(m_transferPort)
        .toUtf8();

    sendMessage(message);

    // 多地址广播关闭时，向已知 peer 单播确保广播不可达时对方仍能收到心跳
    if (!m_multiAddressBroadcast) {
        for (const auto& peer : m_peers) {
            m_socket->writeDatagram(message, peer.address, Constants::DISCOVERY_PORT);
        }
    }
}

void PeerDiscovery::sendMessage(const QByteArray& message) {
    if (m_multiAddressBroadcast) {
        for (const QHostAddress& addr : broadcastAddresses()) {
            m_socket->writeDatagram(message, addr, Constants::DISCOVERY_PORT);
        }
    } else {
        m_socket->writeDatagram(message, QHostAddress::Broadcast, Constants::DISCOVERY_PORT);
    }
}

void PeerDiscovery::onReadyRead() {
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        QByteArray data = datagram.data();
        QHostAddress sender = datagram.senderAddress();

        qDebug() << "UDP recv from" << sender << ":" << data;

        parseMessage(data, sender);
    }
}

void PeerDiscovery::parseMessage(const QByteArray& data, const QHostAddress& sender) {
    QString message = QString::fromUtf8(data);
    QStringList parts = message.split('|');

    if (parts.isEmpty()) return;

    QString type = parts[0];

    // 验证 groupCode 匹配
    if (parts.size() > 1 && parts[1] != m_groupCode) {
        return;
    }

    if (type == QStringLiteral("ANNOUNCE")) {
        // ANNOUNCE|groupCode|deviceId|deviceName|transferPort
        if (parts.size() < 5) return;

        QString deviceId = parts[2];
        QString deviceName = parts[3];
        int transferPort = parts[4].toInt();

        // 忽略自己的广播
        if (deviceId == m_deviceId) return;

        qint64 now = QDateTime::currentMSecsSinceEpoch();
        bool isNewPeer = !m_peers.contains(deviceId);
        bool portChanged = false;

        if (isNewPeer) {
            PeerInfo info;
            info.deviceId = deviceId;
            info.deviceName = deviceName;
            info.address = sender;
            info.transferPort = transferPort;
            info.lastSeen = now;
            m_peers[deviceId] = info;
        } else {
            portChanged = m_peers[deviceId].transferPort != transferPort;
            m_peers[deviceId].deviceName = deviceName;
            m_peers[deviceId].lastSeen = now;
            m_peers[deviceId].address = sender;
            m_peers[deviceId].transferPort = transferPort;
        }

        // 新设备或端口变更时通知上层
        if (isNewPeer || portChanged) {
            emit peerFound(deviceId, deviceName, sender, transferPort);
        }

        // 收到新设备的 ANNOUNCE 时，单播回复自己的 ANNOUNCE，
        // 确保对方也能发现自己（解决虚拟机网络广播不通的问题）
        if (isNewPeer) {
            sendAnnouncementTo(sender);
        }

    } else if (type == QStringLiteral("GOODBYE")) {
        // GOODBYE|groupCode|deviceId|deviceName
        if (parts.size() < 3) return;

        QString deviceId = parts[2];
        if (deviceId == m_deviceId) return;

        if (m_peers.contains(deviceId)) {
            m_peers.remove(deviceId);
            emit peerLost(deviceId);
        }

    } else if (type == QStringLiteral("FILE_ADD")) {
        // FILE_ADD|groupCode|deviceId|deviceName|fileId|fileName|fileSize|shareType|fileCount
        if (parts.size() < 7) return;

        QString deviceId = parts[2];
        if (deviceId == m_deviceId) return;

        QString deviceName = parts[3];
        QString fileId = parts[4];
        QString fileName = parts[5];
        qint64 fileSize = parts[6].toLongLong();

        SharedFileInfo info;
        info.fileId = fileId;
        info.fileName = fileName;
        info.fileSize = fileSize;
        info.deviceId = deviceId;
        info.deviceName = deviceName;
        info.isLocal = false;

        // Parse optional shareType and fileCount (backward compatible)
        if (parts.size() >= 9) {
            info.shareType = static_cast<ShareType>(parts[7].toInt());
            info.fileCount = parts[8].toInt();
        }

        emit fileShared(info);

    } else if (type == QStringLiteral("FILE_REMOVE")) {
        // FILE_REMOVE|groupCode|deviceId|fileId
        if (parts.size() < 4) return;

        QString deviceId = parts[2];
        if (deviceId == m_deviceId) return;

        QString fileId = parts[3];

        emit fileRemoved(fileId, deviceId);
    }
}

void PeerDiscovery::checkPeerTimeouts() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList timedOutPeers;

    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        qint64 elapsed = now - it.value().lastSeen;
        if (elapsed > Constants::PEER_TIMEOUT_MS) {
            timedOutPeers.append(it.key());
        }
    }

    for (const QString& deviceId : timedOutPeers) {
        m_peers.remove(deviceId);
        emit peerLost(deviceId);
    }
}

void PeerDiscovery::announceFile(const SharedFileInfo& file) {
    m_localFiles[file.fileId] = file;

    QByteArray message = QStringLiteral("FILE_ADD|%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(m_groupCode)
        .arg(m_deviceId)
        .arg(m_deviceName.isEmpty() ? AppSettings::instance().deviceName() : m_deviceName)
        .arg(file.fileId)
        .arg(file.fileName)
        .arg(file.fileSize)
        .arg(static_cast<int>(file.shareType))
        .arg(file.fileCount)
        .toUtf8();

    sendMessage(message);
}

void PeerDiscovery::removeFile(const QString& fileId) {
    m_localFiles.remove(fileId);

    QByteArray message = QStringLiteral("FILE_REMOVE|%1|%2|%3")
        .arg(m_groupCode)
        .arg(m_deviceId)
        .arg(fileId)
        .toUtf8();

    sendMessage(message);
}

QList<PeerInfo> PeerDiscovery::peers() const {
    return m_peers.values();
}

void PeerDiscovery::sendAnnouncementTo(const QHostAddress& target) {
    if (m_deviceName.isEmpty()) {
        m_deviceName = AppSettings::instance().deviceName();
    }
    QByteArray message = QStringLiteral("ANNOUNCE|%1|%2|%3|%4")
        .arg(m_groupCode)
        .arg(m_deviceId)
        .arg(m_deviceName)
        .arg(m_transferPort)
        .toUtf8();
    m_socket->writeDatagram(message, target, Constants::DISCOVERY_PORT);
}
