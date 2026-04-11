#include "shared_file.h"
#include <QCryptographicHash>
#include <QFile>

QByteArray SharedFileInfo::toBroadcastData() const {
    // Format: FILE_ADD|groupCode|deviceName|fileId|fileName|fileSize
    // Note: groupCode is added by PeerDiscovery, here we return the rest
    QByteArray data;
    data.append(deviceName.toUtf8());
    data.append('|');
    data.append(fileId.toUtf8());
    data.append('|');
    data.append(fileName.toUtf8());
    data.append('|');
    data.append(QByteArray::number(fileSize));
    return data;
}

SharedFileInfo SharedFileInfo::fromBroadcastData(const QByteArray& data) {
    SharedFileInfo info;
    QList<QByteArray> parts = data.split('|');
    if (parts.size() >= 4) {
        info.deviceName = QString::fromUtf8(parts[0]);
        info.fileId = QString::fromUtf8(parts[1]);
        info.fileName = QString::fromUtf8(parts[2]);
        info.fileSize = parts[3].toLongLong();
        info.isLocal = false;
    }
    return info;
}

SharedFileInfo SharedFileInfo::createLocalFile(const QString& fileName,
                                                const QString& filePath,
                                                qint64 fileSize,
                                                const QString& deviceId,
                                                const QString& deviceName) {
    SharedFileInfo info;
    info.fileId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    info.fileName = fileName;
    info.filePath = filePath;
    info.fileSize = fileSize;
    info.deviceId = deviceId;
    info.deviceName = deviceName;
    info.isLocal = true;
    return info;
}
