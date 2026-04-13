#pragma once
#include <QHBoxLayout>
#include <QHostAddress>
#include <QLabel>
#include <QMainWindow>
#include <QMap>
#include <QPair>
#include <QPushButton>
#include <QSvgRenderer>
#include <QTranslator>
#include <QVBoxLayout>
#include <QWidget>

// 前向声明
class DropAreaWidget;
class FileListWidget;
class SystemTray;
class PeerDiscovery;
class FileTransferManager;
class BreathingDot;

#include "core/shared_file.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void initialize();
    void setTranslator(QTranslator *translator);

  protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

  private slots:
    void onFilesDropped(const QList<QUrl> &urls);
    void onPeerFound(const QString &deviceId, const QString &deviceName,
                     const QHostAddress &addr, int port);
    void onPeerLost(const QString &deviceId);
    void onFileShared(const SharedFileInfo &file);
    void onFileRemoved(const QString &fileId, const QString &deviceId);
    void onDownloadProgress(const QString &fileId, qint64 received,
                            qint64 total);
    void onDownloadComplete(const QString &fileId, const QString &path);
    void onDownloadError(const QString &fileId, const QString &error);
    void onDebugLogToggled(bool enabled);
    void onChangeLanguage(const QString &lang);
    void onChangeCode();

  private:
    // UI 组件
    QWidget *m_centralWidget = nullptr;
    QVBoxLayout *m_mainLayout = nullptr;
    QWidget *m_titleBar = nullptr;
    QPushButton *m_closeButton = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_onlineLabel = nullptr;
    BreathingDot *m_breathingDot = nullptr;
    QLabel *m_separatorLabel = nullptr;
    DropAreaWidget *m_dropArea = nullptr;
    FileListWidget *m_fileList = nullptr;
    SystemTray *m_trayIcon = nullptr;

    // 网络组件
    PeerDiscovery *m_discovery = nullptr;
    FileTransferManager *m_transferManager = nullptr;

    // 翻译
    QTranslator *m_translator = nullptr;

    // 数据
    QMap<QString, SharedFileInfo> m_localSharedFiles;
    QMap<QString, QPair<QHostAddress, int>> m_peerTransferPorts;
    QString m_currentLanguage;

    // 窗口拖拽
    bool m_dragging = false;
    QPoint m_dragStartPos;

    // 标记强制退出（跳过 closeEvent 的隐藏逻辑）
    bool m_forceQuit = false;

    // SVG 背景渲染器
    QSvgRenderer *m_bgRenderer = nullptr;

    void setupUI();
    void setupTitleBar();
    void setupConnections();
    void applyStylesheet();
    void retranslateUi();
    void checkFirstRun();
    void updateOnlineCount();
};
