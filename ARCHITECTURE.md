# ShareArea - Architecture Design

## Overview
跨设备局域网文件分享工具，基于 Qt 6.10 C++ 实现，支持 Windows 和 macOS。

## Protocol Design

### Discovery (UDP port 19876)
- 定期广播：`ANNOUNCE|groupCode|deviceName|transferPort`
- 退出通知：`GOODBYE|groupCode|deviceName`
- 文件分享：`FILE_ADD|groupCode|deviceName|fileId|fileName|fileSize`
- 文件移除：`FILE_REMOVE|groupCode|deviceName|fileId`

### File Transfer (TCP)
- 服务端监听动态端口（通过 discovery 广播告知）
- 客户端请求：`GET|fileId\n`
- 服务端响应：`OK|fileSize\n` + 原始文件数据
- 进度通过 bytesReceived/totalBytes 计算

## Constants (src/constants.h)
```cpp
#pragma once
#include <QString>

namespace Constants {
    constexpr int DISCOVERY_PORT = 19876;
    constexpr int DEFAULT_TRANSFER_PORT = 19877;
    constexpr int BROADCAST_INTERVAL_MS = 3000;
    constexpr int PEER_TIMEOUT_MS = 10000;
    constexpr int TRANSFER_CHUNK_SIZE = 65536;
    const QString APP_NAME = "ShareArea";
    const QString APP_VERSION = "1.0.0";
}
```

## Data Models (src/core/shared_file.h)
```cpp
#pragma once
#include <QString>
#include <QMetaType>

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
};
Q_DECLARE_METATYPE(SharedFileInfo)
```

## Settings (src/core/app_settings.h)
```cpp
#pragma once
#include <QObject>
#include <QSettings>

class AppSettings : public QObject {
    Q_OBJECT
public:
    static AppSettings& instance();

    QString groupCode() const;
    void setGroupCode(const QString& code);
    bool hasGroupCode() const;

    QString language() const;
    void setLanguage(const QString& lang);

    QString deviceName() const;
    void setDeviceName(const QString& name);

    void save();
    void load();

private:
    AppSettings();
    QSettings m_settings;
};
```

## Network Interfaces

### PeerDiscovery (src/network/peer_discovery.h)
```cpp
#pragma once
#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QHostAddress>
#include <QMap>
#include "shared_file.h"

struct PeerInfo {
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
    QList<PeerInfo> peers() const;

signals:
    void peerFound(const QString& deviceName, const QHostAddress& address, int transferPort);
    void peerLost(const QString& deviceName);
    void fileShared(const SharedFileInfo& file);
    void fileRemoved(const QString& fileId, const QString& deviceId);

private slots:
    void onReadyRead();
    void sendAnnouncement();

private:
    QUdpSocket* m_socket;
    QTimer* m_broadcastTimer;
    QTimer* m_timeoutTimer;
    QString m_groupCode;
    QMap<QString, PeerInfo> m_peers;  // deviceName -> PeerInfo
    QList<SharedFileInfo> m_localFiles;
    int m_transferPort = 0;

    void setupSocket();
    void parseMessage(const QByteArray& data, const QHostAddress& sender);
    void checkPeerTimeouts();
    void sendMessage(const QByteArray& message);
};
```

### FileTransferManager (src/network/file_transfer_manager.h)
```cpp
#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include "shared_file.h"

struct TransferInfo {
    QString fileId;
    QString fileName;
    QString filePath;     // source (upload) or dest (download)
    qint64 fileSize = 0;
    qint64 bytesTransferred = 0;
    bool isUpload = false;
    QTcpSocket* socket = nullptr;
};

class FileTransferManager : public QObject {
    Q_OBJECT
public:
    explicit FileTransferManager(QObject* parent = nullptr);
    ~FileTransferManager();

    bool startServer(int preferredPort = 0);
    void stopServer();
    int transferPort() const;

    void setLocalFiles(const QMap<QString, SharedFileInfo>& files);
    void requestFile(const SharedFileInfo& fileInfo, const QString& savePath,
                     const QHostAddress& peerAddress, int peerPort);

signals:
    void downloadProgress(const QString& fileId, qint64 received, qint64 total);
    void downloadComplete(const QString& fileId, const QString& savePath);
    void downloadError(const QString& fileId, const QString& error);
    void uploadStarted(const QString& fileId);
    void uploadProgress(const QString& fileId, qint64 sent, qint64 total);
    void uploadComplete(const QString& fileId);

private slots:
    void onNewConnection();
    void onReadyRead();

private:
    QTcpServer* m_server;
    QMap<QString, SharedFileInfo> m_localFiles;  // fileId -> info
    QMap<QTcpSocket*, TransferInfo> m_activeTransfers;
    QMap<QTcpSocket*, QByteArray> m_buffers;

    void handleClientRequest(QTcpSocket* socket);
    void handleDownloadResponse(QTcpSocket* socket);
    void sendFileChunk(QTcpSocket* socket);
};
```

## UI Interfaces

### SetupDialog (src/ui/setup_dialog.h)
```cpp
#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

class SetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit SetupDialog(QWidget* parent = nullptr);
    QString groupCode() const;

private:
    QLineEdit* m_codeEdit;
    QPushButton* m_okButton;
    void setupUI();
};
```

### DropAreaWidget (src/ui/drop_area_widget.h)
```cpp
#pragma once
#include <QWidget>
#include <QLabel>

class DropAreaWidget : public QWidget {
    Q_OBJECT
public:
    explicit DropAreaWidget(QWidget* parent = nullptr);

signals:
    void filesDropped(const QList<QUrl>& urls);
    void fileCancelled(const QString& fileId);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QLabel* m_hintLabel;
    bool m_dragActive = false;
    void setupUI();
};
```

### FileListWidget (src/ui/file_list_widget.h)
```cpp
#pragma once
#include <QWidget>
#include <QListWidget>
#include <QMap>
#include "shared_file.h"

class FileListWidget : public QWidget {
    Q_OBJECT
public:
    explicit FileListWidget(QWidget* parent = nullptr);

    void addFile(const SharedFileInfo& file);
    void removeFile(const QString& fileId, const QString& deviceId);
    void clearRemoteFiles(const QString& deviceId);

    void updateTransferProgress(const QString& fileId, qint64 received, qint64 total);

signals:
    void fileDownloadRequested(const SharedFileInfo& file, const QString& savePath);

private slots:
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    QListWidget* m_listWidget;
    QMap<QString, QListWidgetItem*> m_items;  // fileId -> item
    QMap<QString, SharedFileInfo> m_fileInfos;

    void setupUI();
};
```

### SystemTray (src/ui/system_tray.h)
```cpp
#pragma once
#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

class SystemTray : public QObject {
    Q_OBJECT
public:
    explicit SystemTray(QObject* parent = nullptr);
    void show();

signals:
    void showWindowRequested();
    void changeLanguageRequested(const QString& lang);
    void changeCodeRequested();
    void quitRequested();

private:
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_contextMenu;
    QAction* m_showAction;
    QMenu* m_languageMenu;
    QAction* m_chineseAction;
    QAction* m_englishAction;
    QAction* m_changeCodeAction;
    QAction* m_quitAction;

    void setupMenu();
};
```

### MainWindow (src/ui/main_window.h)
```cpp
#pragma once
#include <QMainWindow>
#include <QVBoxLayout>
#include "drop_area_widget.h"
#include "file_list_widget.h"
#include "system_tray.h"
#include "peer_discovery.h"
#include "file_transfer_manager.h"
#include "app_settings.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void initialize();  // Called after show, sets up network

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onFilesDropped(const QList<QUrl>& urls);
    void onPeerFound(const QString& name, const QHostAddress& addr, int port);
    void onPeerLost(const QString& name);
    void onFileShared(const SharedFileInfo& file);
    void onFileRemoved(const QString& fileId, const QString& deviceId);
    void onDownloadProgress(const QString& fileId, qint64 received, qint64 total);
    void onDownloadComplete(const QString& fileId, const QString& path);
    void onDownloadError(const QString& fileId, const QString& error);
    void onChangeLanguage(const QString& lang);
    void onChangeCode();

private:
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    DropAreaWidget* m_dropArea;
    FileListWidget* m_fileList;
    SystemTray* m_trayIcon;

    PeerDiscovery* m_discovery;
    FileTransferManager* m_transferManager;

    QMap<QString, SharedFileInfo> m_localSharedFiles;  // fileId -> info
    QMap<QString, QPair<QHostAddress, int>> m_peerTransferPorts;  // deviceName -> (addr, port)

    void setupUI();
    void setupConnections();
    void applyStylesheet();
    void retranslateUi();
};
```

## main.cpp
```cpp
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include "main_window.h"
#include "app_settings.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("ShareArea");
    app.setOrganizationName("ShareArea");

    // Load translation
    QTranslator translator;
    AppSettings::instance().load();
    QString lang = AppSettings::instance().language();
    if (lang.isEmpty()) lang = QLocale::system().name();
    translator.load(":/translations/share_area_" + lang);
    app.installTranslator(&translator);

    MainWindow window;
    window.show();

    return app.exec();
}
```

## Build (CMakeLists.txt)
- Qt 6.10, C++17
- Qt modules: Core, Gui, Widgets, Network, LinguistTools
- CMAKE_PREFIX_PATH: /Users/yajiang/Qt6.10/macos

## Modern UI Design Guidelines
- 圆角矩形 (border-radius: 8-12px)
- 柔和阴影
- 浅色主题为主，配色使用蓝色系 (#3498db, #2980b9)
- Drop area 使用虚线边框，hover 时高亮
- 文件列表使用卡片式设计
- 进度条使用渐变色
- 字体: system default, 标题 16pt, 正文 12pt
