#pragma once
#include <QString>
#include <QMetaType>
#include <QUuid>
#include <QByteArray>

struct SharedFileInfo {
    QString fileId;
    QString fileName;
    QString filePath;      // local absolute path (only for local files)
    qint64 fileSize = 0;
    QString deviceId;
    QString deviceName;
    bool isLocal = false;

    // For serialization in protocol
    QByteArray toBroadcastData() const;
    static SharedFileInfo fromBroadcastData(const QByteArray& data);

    static SharedFileInfo createLocalFile(const QString& fileName,
                                          const QString& filePath,
                                          qint64 fileSize,
                                          const QString& deviceId,
                                          const QString& deviceName);
};

Q_DECLARE_METATYPE(SharedFileInfo)
