#pragma once
#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QHostAddress>
#include <QMap>
#include <QDateTime>
#include <QList>
#include "core/shared_file.h"
#include "constants.h"

struct PeerInfo {
    QString deviceId;
    QString deviceName;
    QHostAddress address;
    int transferPort = 0;
    qint64 lastSeen = 0;
};

class PeerDiscovery : public QObject {
    Q_OBJECT

public:
    explicit PeerDiscovery(const QString& groupCode, QObject* parent = nullptr);
    ~PeerDiscovery();

    void start();
    void stop();
    void announceFile(const SharedFileInfo& file);
    void removeFile(const QString& fileId);
    void setTransferPort(int port);
    QList<PeerInfo> peers() const;
    void setMultiAddressBroadcast(bool on);

signals:
    void peerFound(const QString& deviceId, const QString& deviceName, const QHostAddress& address, int transferPort);
    void peerLost(const QString& deviceId);
    void fileShared(const SharedFileInfo& file);
    void fileRemoved(const QString& fileId, const QString& deviceId);

private slots:
    void onReadyRead();
    void sendAnnouncement();
    void checkPeerTimeouts();

private:
    QUdpSocket* m_socket;
    QTimer* m_broadcastTimer;
    QTimer* m_timeoutTimer;
    QString m_groupCode;
    QString m_deviceId;
    QString m_deviceName;
    QMap<QString, PeerInfo> m_peers;  // deviceId -> PeerInfo
    QMap<QString, SharedFileInfo> m_localFiles;  // fileId -> info
    QList<QHostAddress> m_broadcastAddresses;
    int m_transferPort = 0;

    void setupSocket();
    void parseMessage(const QByteArray& data, const QHostAddress& sender);
    void sendMessage(const QByteArray& message);
    void refreshBroadcastAddresses();
};
