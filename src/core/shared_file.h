#pragma once
#include <QString>
#include <QMetaType>
#include <QUuid>
#include <QByteArray>

// Share type: 0=file, 1=directory, 2=clipboard text, 3=clipboard image
enum class ShareType : int {
    File = 0,
    Directory = 1,
    Clipboard = 2,
    ClipboardImage = 3,
};

struct SharedFileInfo {
    QString fileId;
    QString fileName;
    QString filePath;       // local absolute path (only for local files)
    QString localSavePath;  // where remote file was saved locally after download
    qint64 fileSize = 0;
    QString deviceId;
    QString deviceName;
    bool isLocal = false;
    ShareType shareType = ShareType::File;
    int fileCount = 0;          // number of files in directory
    QByteArray rawData;         // clipboard raw data: text UTF-8 or image PNG (memory only, not serialized)

    // Convenience helpers
    bool isDirectory() const { return shareType == ShareType::Directory; }
    bool isClipboard() const { return shareType == ShareType::Clipboard || shareType == ShareType::ClipboardImage; }
    bool isClipboardImage() const { return shareType == ShareType::ClipboardImage; }

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
