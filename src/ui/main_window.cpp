#include "main_window.h"
#include "core/app_settings.h"
#include "network/file_transfer_manager.h"
#include "network/peer_discovery.h"
#include "ui/app_icon.h"
#include "ui/breathing_dot.h"
#include "ui/drop_area_widget.h"
#include "ui/file_list_widget.h"
#include "ui/setup_dialog.h"
#include "ui/system_tray.h"
#include "ui/log_window.h"

#ifdef Q_OS_MACOS
#include "ui/macos_blur.h"
#endif
#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHostInfo>
#include <QLinearGradient>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QRadialGradient>
#include <QStandardPaths>
#include <QDir>
#include <QSvgRenderer>
#include <QUuid>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // 无边框窗口
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_TranslucentBackground);

    // 初始化 SVG 背景渲染器
    m_bgRenderer =
        new QSvgRenderer(QStringLiteral(":/images/window-bg.svg"), this);

    setupUI();
    applyStylesheet();

    AppSettings::instance().load();
    m_currentLanguage = AppSettings::instance().language();
}

void MainWindow::setTranslator(QTranslator *translator) {
    m_translator = translator;
}

MainWindow::~MainWindow() {
    if (m_discovery)
        m_discovery->stop();
    if (m_transferManager)
        m_transferManager->stopServer();
}

// ---- 绘制背景 + 圆角 ----
void MainWindow::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect rect = this->rect();
    qreal r = 12.0;

    QPainterPath clip;
    clip.addRoundedRect(rect, r, r);
    p.setClipPath(clip);

    // 使用 SVG 背景渲染
    if (m_bgRenderer && m_bgRenderer->isValid()) {
        m_bgRenderer->render(&p, rect);
    } else {
        // 回退：不透明暖白渐变背景，透明度由 setWindowOpacity() 统一控制
        QLinearGradient grad(QPointF(0, 0),
                             QPointF(rect.width() * 0.6, rect.height()));
        grad.setColorAt(0.0, QColor("#fafaf9"));
        grad.setColorAt(0.5, QColor("#f5f5f4"));
        grad.setColorAt(1.0, QColor("#f0ede8"));
        p.fillRect(rect, grad);

        // 装饰光晕 1（左上，暖琥珀）
        QRadialGradient glow1(QPointF(rect.width() * 0.1, -20), 240);
        glow1.setColorAt(0.0, QColor(251, 191, 36, 30));
        glow1.setColorAt(1.0, QColor(251, 191, 36, 0));
        p.fillRect(rect, glow1);

        // 装饰光晕 2（右下，淡青）
        QRadialGradient glow2(QPointF(rect.width() * 0.9, rect.height() + 20),
                              260);
        glow2.setColorAt(0.0, QColor(148, 163, 184, 20));
        glow2.setColorAt(1.0, QColor(148, 163, 184, 0));
        p.fillRect(rect, glow2);
    }
}

// ---- 自定义标题栏 ----
void MainWindow::setupTitleBar() {
    m_titleBar = new QWidget(this);
    m_titleBar->setFixedHeight(42);
    m_titleBar->setCursor(Qt::ArrowCursor);

    auto *layout = new QHBoxLayout(m_titleBar);
    layout->setContentsMargins(14, 0, 10, 0);
    layout->setSpacing(8);

    // Logo + 标题
    QIcon logoIcon(QStringLiteral(":/icons/logo.svg"));
    auto *logoLabel = new QLabel(this);
    logoLabel->setPixmap(logoIcon.pixmap(22, 22));
    layout->addWidget(logoLabel);

    m_titleLabel = new QLabel(QStringLiteral("ShareArea"), this);
    m_titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #292524; font-size: 13px; font-weight: 600; }"));
    layout->addWidget(m_titleLabel);

    // 呼吸灯
    m_breathingDot = new BreathingDot(this);
    layout->addWidget(m_breathingDot);

    // 在线数
    m_onlineLabel = new QLabel(tr("%n 在线", nullptr, 0), this);
    m_onlineLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: rgba(41,37,36,0.50); font-size: 11px; }"));
    layout->addWidget(m_onlineLabel);

    layout->addStretch();

    // 关闭按钮
    m_closeButton = new QPushButton(this);
    m_closeButton->setFixedSize(28, 28);
    m_closeButton->setToolTip(tr("关闭"));
    m_closeButton->setIcon(QIcon(QStringLiteral(":/icons/close.svg")));
    m_closeButton->setIconSize(QSize(14, 14));
    m_closeButton->installEventFilter(this);
    layout->addWidget(m_closeButton);

    connect(m_closeButton, &QPushButton::clicked, this, [this]() {
        if (m_trayIcon && m_trayIcon->isVisible()) {
            hide();
        }
    });
}

void MainWindow::setupUI() {
    setWindowTitle(QStringLiteral("ShareArea"));
    setWindowIcon(createAppIcon());
    resize(380, 480);
    setMinimumSize(340, 380);

    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    auto *rootLayout = new QVBoxLayout(m_centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // 标题栏
    setupTitleBar();
    rootLayout->addWidget(m_titleBar);

    // 内容区域
    auto *contentWidget = new QWidget(this);
    m_mainLayout = new QVBoxLayout(contentWidget);
    m_mainLayout->setSpacing(12);
    m_mainLayout->setContentsMargins(20, 12, 20, 20);

    // 拖放区域
    m_dropArea = new DropAreaWidget(this);
    m_mainLayout->addWidget(m_dropArea, 0, Qt::AlignTop);

    // 分隔标签
    m_separatorLabel = new QLabel(tr("其他设备分享的文件"), this);
    QFont sepFont = m_separatorLabel->font();
    sepFont.setPointSize(11);
    sepFont.setBold(true);
    m_separatorLabel->setFont(sepFont);
    m_separatorLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: rgba(41,37,36,0.40); padding: 4px 0px; }"));
    m_mainLayout->addWidget(m_separatorLabel, 0, Qt::AlignTop);

    // 文件列表
    m_fileList = new FileListWidget(this);
    m_mainLayout->addWidget(m_fileList, 1);

    rootLayout->addWidget(contentWidget, 1);

    // 系统托盘
    m_trayIcon = new SystemTray(this);
    m_trayIcon->setToolTip(QStringLiteral("ShareArea"));
    m_trayIcon->show();
}

void MainWindow::initialize() {
    checkFirstRun();
    QString groupCode = AppSettings::instance().groupCode();
    m_titleLabel->setText(QStringLiteral("ShareArea · %1").arg(groupCode));

    // 启用 macOS 原生毛玻璃效果
#ifdef Q_OS_MACOS
    if (windowHandle()) {
        enableMacOSBlur(windowHandle());
    } else {
        QMetaObject::invokeMethod(
            this,
            [this]() {
                if (windowHandle())
                    enableMacOSBlur(windowHandle());
            },
            Qt::QueuedConnection);
    }
#endif

    // 恢复保存的窗口透明度
    setWindowOpacity(AppSettings::instance().opacity() / 100.0);

    // 先启动文件传输服务器，获取端口号
    m_transferManager = new FileTransferManager(this);
    if (!m_transferManager->startServer(Constants::DEFAULT_TRANSFER_PORT)) {
        qWarning() << "Failed to start transfer server on default port, trying "
                      "random port";
        m_transferManager->startServer(0);
    }
    m_transferManager->setLocalFiles(m_localSharedFiles);

    // 再启动设备发现，确保广播时已携带正确端口
    m_discovery = new PeerDiscovery(groupCode, this);
    m_discovery->setTransferPort(m_transferManager->transferPort());
    m_discovery->start();

    setupConnections();

    // 设置语言菜单初始选中
    m_trayIcon->updateLanguageChecked(m_currentLanguage);

    // 默认窗口置顶
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    show();
}

void MainWindow::setupConnections() {
    connect(m_dropArea, &DropAreaWidget::filesDropped, this,
            &MainWindow::onFilesDropped);

    connect(m_discovery, &PeerDiscovery::peerFound, this,
            &MainWindow::onPeerFound);
    connect(m_discovery, &PeerDiscovery::peerLost, this,
            &MainWindow::onPeerLost);
    connect(m_discovery, &PeerDiscovery::fileShared, this,
            &MainWindow::onFileShared);
    connect(m_discovery, &PeerDiscovery::fileRemoved, this,
            &MainWindow::onFileRemoved);

    connect(m_transferManager, &FileTransferManager::downloadProgress, this,
            &MainWindow::onDownloadProgress);
    connect(m_transferManager, &FileTransferManager::downloadComplete, this,
            &MainWindow::onDownloadComplete);
    connect(m_transferManager, &FileTransferManager::downloadError, this,
            &MainWindow::onDownloadError);

    connect(m_fileList, &FileListWidget::fileDownloadRequested, this,
            [this](const SharedFileInfo &file, const QString &savePath) {
                // savePath 非空且文件已存在 → 已下载过，直接打开 Finder 定位
                if (!file.localSavePath.isEmpty() && QFile::exists(file.localSavePath)) {
#ifdef Q_OS_MACOS
                    QProcess::startDetached(QStringLiteral("open"),
                        {QStringLiteral("-R"), file.localSavePath});
#elif defined(Q_OS_WIN)
                    QProcess::startDetached(QStringLiteral("explorer"),
                        {QStringLiteral("/select,"), QDir::toNativeSeparators(file.localSavePath)});
#endif
                    return;
                }
                auto it = m_peerTransferPorts.find(file.deviceId);
                if (it != m_peerTransferPorts.end()) {
                    m_transferManager->requestFile(
                        file, savePath, it.value().first, it.value().second);
                } else {
                    qWarning()
                        << "No transfer info for device:" << file.deviceId;
                }
            });

    connect(m_fileList, &FileListWidget::fileDeleteRequested, this,
            [this](const QString &fileId) {
                m_fileList->removeFile(fileId);
            });

    connect(m_fileList, &FileListWidget::fileCancelRequested, this,
            [this](const QString &fileId) {
                m_transferManager->cancelDownload(fileId);
                m_fileList->resetDownload(fileId);
            });

    connect(m_trayIcon, &SystemTray::showWindowRequested, this, [this]() {
        show();
        raise();
        activateWindow();
    });
    connect(m_trayIcon, &SystemTray::changeLanguageRequested, this,
            &MainWindow::onChangeLanguage);
    connect(m_trayIcon, &SystemTray::changeCodeRequested, this,
            &MainWindow::onChangeCode);
    connect(m_trayIcon, &SystemTray::debugLogToggled, this,
            &MainWindow::onDebugLogToggled);
    connect(m_trayIcon, &SystemTray::alwaysOnTopChanged, this, [this](bool on) {
        setWindowFlag(Qt::WindowStaysOnTopHint, on);
        show();
    });
    connect(m_trayIcon, &SystemTray::opacityChanged, this, [this](int value) {
        setWindowOpacity(value / 100.0);
        AppSettings::instance().setOpacity(value);
        AppSettings::instance().save();
    });
    connect(m_trayIcon, &SystemTray::quitRequested, this, [this]() {
        m_forceQuit = true;
        if (m_discovery)
            m_discovery->stop();
        if (m_transferManager)
            m_transferManager->stopServer();
        close();
        QApplication::quit();
    });

    // macOS 菜单栏退出也会触发 aboutToQuit，确保清理资源
    connect(qApp, &QApplication::aboutToQuit, this, [this]() {
        if (m_discovery)
            m_discovery->stop();
        if (m_transferManager)
            m_transferManager->stopServer();
    });
}

// ---- 样式表 ----
void MainWindow::applyStylesheet() {
    setStyleSheet(R"(
        /* 标题栏按钮 */
        QPushButton {
            background: transparent;
            color: #78716c;
            border: none;
            border-radius: 14px;
            font-size: 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background: rgba(239,68,68,0.10);
            color: #dc2626;
        }

        /* 进度条 */
        QProgressBar {
            border-radius: 4px;
            text-align: center;
            background: rgba(231,229,228,0.7);
            border: none;
            max-height: 6px;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                stop:0 #d97706, stop:1 #f59e0b);
            border-radius: 4px;
        }

        /* 输入框 */
        QLineEdit {
            border: 1.5px solid rgba(168,162,158,0.35);
            border-radius: 8px;
            padding: 8px 12px;
            font-size: 13px;
            background: rgba(255,255,255,0.70);
            color: #292524;
        }
        QLineEdit:focus {
            border-color: #d97706;
            background: rgba(255,255,255,0.90);
        }
    )");
}

// ---- 窗口拖拽 ----
void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && event->pos().y() < 42) {
        m_dragging = true;
        m_dragStartPos =
            event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragStartPos);
        event->accept();
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    m_dragging = false;
    event->accept();
}

// ---- 事件 ----
bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_closeButton) {
        if (event->type() == QEvent::Enter) {
            m_closeButton->setIcon(
                QIcon(QStringLiteral(":/icons/close-hover.svg")));
        } else if (event->type() == QEvent::Leave) {
            m_closeButton->setIcon(QIcon(QStringLiteral(":/icons/close.svg")));
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_trayIcon && m_trayIcon->isVisible() && !m_forceQuit) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized() && m_trayIcon) {
            QTimer::singleShot(0, this, [this]() {
                if (isMinimized())
                    hide();
            });
        }
    }
}

// ---- 业务逻辑 ----
void MainWindow::checkFirstRun() {
    if (!AppSettings::instance().hasGroupCode()) {
        SetupDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            AppSettings::instance().setGroupCode(dialog.groupCode());
            AppSettings::instance().setDeviceName(QHostInfo::localHostName());
            AppSettings::instance().save();
        } else {
            AppSettings::instance().setGroupCode(QStringLiteral("default"));
            AppSettings::instance().setDeviceName(QHostInfo::localHostName());
            AppSettings::instance().save();
        }
    }
}

void MainWindow::onFilesDropped(const QList<QUrl> &urls) {
    QString deviceName = AppSettings::instance().deviceName();
    for (const QUrl &url : urls) {
        QString filePath = url.toLocalFile();
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists() || !fileInfo.isFile())
            continue;

        SharedFileInfo sharedFile = SharedFileInfo::createLocalFile(
            fileInfo.fileName(), filePath, fileInfo.size(), deviceName,
            deviceName);
        m_localSharedFiles[sharedFile.fileId] = sharedFile;

        if (m_transferManager)
            m_transferManager->setLocalFiles(m_localSharedFiles);
        if (m_discovery)
            m_discovery->announceFile(sharedFile);

        qDebug() << "Shared file:" << sharedFile.fileName
                 << "ID:" << sharedFile.fileId;
    }
}

void MainWindow::onPeerFound(const QString &deviceId, const QString &deviceName,
                             const QHostAddress &addr, int port) {
    bool isNew = !m_peerTransferPorts.contains(deviceId);
    qDebug() << "Peer found:" << deviceName << addr.toString() << port;
    m_peerTransferPorts[deviceId] = qMakePair(addr, port);
    if (isNew && m_trayIcon)
        m_trayIcon->showMessage(tr("设备上线"),
                                tr("%1 已加入分享组").arg(deviceName));
    updateOnlineCount();
}

void MainWindow::onPeerLost(const QString &deviceId) {
    qDebug() << "Peer lost:" << deviceId;
    m_peerTransferPorts.remove(deviceId);
    m_fileList->clearRemoteFiles(deviceId);
    updateOnlineCount();
}

void MainWindow::onFileShared(const SharedFileInfo &file) {
    qDebug() << "File shared:" << file.fileName
             << "id:" << file.fileId
             << "from" << file.deviceName;
    m_fileList->addFile(file);
}

void MainWindow::onFileRemoved(const QString &fileId, const QString &deviceId) {
    qDebug() << "File removed:" << fileId << "from" << deviceId;
    m_fileList->removeFile(fileId, deviceId);
}

void MainWindow::onDownloadProgress(const QString &fileId, qint64 received,
                                    qint64 total) {
    m_fileList->updateTransferProgress(fileId, received, total);
}

void MainWindow::onDownloadComplete(const QString &fileId,
                                    const QString &path) {
    qDebug() << "Download complete:" << fileId << "at" << path;
    m_fileList->updateFileSavePath(fileId, path);

    // 打开 Finder / 资源管理器定位到文件
#ifdef Q_OS_MACOS
    QProcess::startDetached(QStringLiteral("open"),
        {QStringLiteral("-R"), path});
#elif defined(Q_OS_WIN)
    QProcess::startDetached(QStringLiteral("explorer"),
        {QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
#endif

    if (m_trayIcon)
        m_trayIcon->showMessage(tr("下载完成"), tr("文件已成功下载"));
}

void MainWindow::onDownloadError(const QString &fileId, const QString &error) {
    qWarning() << "Download error:" << fileId << error;

    // 重置列表项的下载状态
    m_fileList->resetDownload(fileId);

    if (error == QStringLiteral("not_found")) {
        // 远端不再持有该 fileId，清理本地失效条目避免重复点击失败
        m_fileList->removeFile(fileId);
        QMessageBox::warning(this, tr("下载失败"),
                             tr("该文件在对端已失效，请等待对方重新分享后重试。"));
        return;
    }

    QMessageBox::warning(this, tr("下载失败"),
                         tr("无法下载文件：%1").arg(error));
}

void MainWindow::onChangeLanguage(const QString &lang) {
    if (m_currentLanguage == lang)
        return;
    m_currentLanguage = lang;
    AppSettings::instance().setLanguage(lang);
    AppSettings::instance().save();

    // 动态切换翻译
    if (m_translator) {
        QApplication::removeTranslator(m_translator);
        (void)(m_translator->load(QStringLiteral("share_area_") + lang,
                                  QApplication::applicationDirPath() +
                                      QStringLiteral("/translations")));
        QApplication::installTranslator(m_translator);
    }

    // 刷新所有 UI 文字
    retranslateUi();
    m_trayIcon->retranslateUi();
    m_trayIcon->updateLanguageChecked(lang);
    m_dropArea->retranslateUi();
    m_fileList->retranslateUi();
}

void MainWindow::onChangeCode() {
    SetupDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString newCode = dialog.groupCode();
        if (m_discovery) {
            m_discovery->stop();
            delete m_discovery;
            m_discovery = nullptr;
        }
        if (m_transferManager) {
            m_transferManager->stopServer();
            delete m_transferManager;
            m_transferManager = nullptr;
        }
        AppSettings::instance().setGroupCode(newCode);
        AppSettings::instance().save();
        m_peerTransferPorts.clear();
        m_localSharedFiles.clear();
        m_fileList->clearRemoteFiles(QString());
        initialize();
    }
}

void MainWindow::retranslateUi() {
    m_separatorLabel->setText(tr("其他设备分享的文件"));
    m_closeButton->setToolTip(tr("关闭"));
    QString groupCode = AppSettings::instance().groupCode();
    m_titleLabel->setText(QStringLiteral("ShareArea · %1").arg(groupCode));
    updateOnlineCount();
}

void MainWindow::updateOnlineCount() {
    int count = m_peerTransferPorts.size();
    m_onlineLabel->setText(tr("%n 在线", nullptr, count));
    m_breathingDot->setActive(count > 0);
}

void MainWindow::onDebugLogToggled(bool enabled) {
    if (enabled) {
        auto* logWin = new LogWindow(nullptr);
        logWin->setAttribute(Qt::WA_DeleteOnClose);
        logWin->setWindowFlags(Qt::Window);
        logWin->show();
    }
    // LogWindow 析构时自动恢复消息处理器
}
