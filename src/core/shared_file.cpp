#include "shared_file.h"
#include <QCryptographicHash>
#include <QFile>

QByteArray SharedFileInfo::toBroadcastData() const {
    // Format: deviceName|fileId|fileName|fileSize|shareType|fileCount
    // shareType: 0=file, 1=directory, 2=clipboard
    // Note: groupCode is added by PeerDiscovery as prefix
    QByteArray data;
    data.append(deviceName.toUtf8());
    data.append('|');
    data.append(fileId.toUtf8());
    data.append('|');
    data.append(fileName.toUtf8());
    data.append('|');
    data.append(QByteArray::number(fileSize));
    data.append('|');
    data.append(QByteArray::number(static_cast<int>(shareType)));
    data.append('|');
    data.append(QByteArray::number(fileCount));
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
        // Parse optional shareType and fileCount (backward compatible)
        if (parts.size() >= 6) {
            info.shareType = static_cast<ShareType>(parts[4].toInt());
            info.fileCount = parts[5].toInt();
        }
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
