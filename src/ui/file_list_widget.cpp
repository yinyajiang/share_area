#include "file_list_widget.h"
#include <QHBoxLayout>
#include <QStyle>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QIcon>
#include <QDebug>

// ==================== FileListItemWidget ====================

FileListItemWidget::FileListItemWidget(const SharedFileInfo& fileInfo, QWidget* parent)
    : QWidget(parent), m_fileInfo(fileInfo) {
    setupUI();
}

void FileListItemWidget::setupUI() {
    auto* topLayout = new QHBoxLayout(this);
    topLayout->setSpacing(8);
    topLayout->setContentsMargins(12, 10, 8, 10);

    // 左侧文件信息
    auto* infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(4);

    // 文件名
    m_nameLabel = new QLabel(m_fileInfo.fileName, this);
    QFont nameFont = m_nameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(12);
    m_nameLabel->setFont(nameFont);
    m_nameLabel->setStyleSheet(QStringLiteral("QLabel { color: #292524; }"));
    infoLayout->addWidget(m_nameLabel);

    // 信息行（大小/文件数 + 来源）
    m_infoLabel = new QLabel(this);
    QFont infoFont = m_infoLabel->font();
    infoFont.setPointSize(10);
    m_infoLabel->setFont(infoFont);
    m_infoLabel->setStyleSheet(QStringLiteral("QLabel { color: #78716c; }"));
    QString infoText;
    if (m_fileInfo.isDirectory) {
        infoText = tr("%1 个文件 · 来自 %2").arg(m_fileInfo.fileCount).arg(m_fileInfo.deviceName);
    } else {
        infoText = tr("%1 · 来自 %2").arg(formatSize(m_fileInfo.fileSize), m_fileInfo.deviceName);
    }
    m_infoLabel->setText(infoText);
    infoLayout->addWidget(m_infoLabel);

    // 进度条（初始隐藏）
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat(tr("%p%"));
    m_progressBar->setMaximumHeight(6);
    m_progressBar->hide();
    infoLayout->addWidget(m_progressBar);

    // 状态标签（初始隐藏）
    m_statusLabel = new QLabel(this);
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(10);
    m_statusLabel->setFont(statusFont);
    m_statusLabel->hide();
    infoLayout->addWidget(m_statusLabel);

    topLayout->addLayout(infoLayout, 1);

    // 右侧删除按钮
    m_deleteButton = new QPushButton(this);
    m_deleteButton->setFixedSize(20, 20);
    m_deleteButton->setCursor(Qt::PointingHandCursor);
    m_deleteButton->setToolTip(tr("删除"));
    m_deleteButton->hide();  // 默认隐藏，hover 时由 QSS 控制

    m_deleteButton->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            border: none;
            border-radius: 10px;
        }
        QPushButton:hover {
            background: rgba(220, 38, 38, 0.12);
        }
    )");

    // 用 SVG 图标：× 符号
    m_deleteButton->setIcon(
        QIcon(QStringLiteral(":/icons/close.svg")));
    m_deleteButton->setIconSize(QSize(12, 12));

    connect(m_deleteButton, &QPushButton::clicked, this, [this]() {
        if (m_downloading) {
            emit cancelRequested(m_fileInfo.fileId);
        } else {
            emit deleteRequested(m_fileInfo.fileId);
        }
    });

    topLayout->addWidget(m_deleteButton);
}

void FileListItemWidget::updateProgress(qint64 received, qint64 total) {
    if (total > 0) {
        int percent = static_cast<int>((received * 100) / total);
        if (m_progressBar->value() != percent) {
            m_progressBar->setValue(percent);
        }
        m_progressBar->show();

        // 下载中显示取消按钮
        if (!m_downloading) {
            m_downloading = true;
            m_deleteButton->setToolTip(tr("取消"));
            m_deleteButton->setStyleSheet(R"(
                QPushButton {
                    background: transparent;
                    border: none;
                    border-radius: 10px;
                }
                QPushButton:hover {
                    background: rgba(220, 38, 38, 0.12);
                }
            )");
            m_deleteButton->show();
        }

        if (received >= total) {
            m_downloading = false;
            m_statusLabel->hide();
            // 恢复删除按钮样式
            m_deleteButton->setToolTip(tr("删除"));
            m_deleteButton->setStyleSheet(R"(
                QPushButton {
                    background: transparent;
                    border: none;
                    border-radius: 10px;
                }
                QPushButton:hover {
                    background: rgba(220, 38, 38, 0.12);
                }
            )");
        } else {
            // 计算下载速度
            if (!m_speedTimer.isValid()) {
                m_speedTimer.start();
                m_lastReceived = received;
            } else {
                qint64 elapsedMs = m_speedTimer.elapsed();
                if (elapsedMs >= 1000) {
                    qint64 bytesDelta = received - m_lastReceived;
                    double speed = bytesDelta / (elapsedMs / 1000.0);  // bytes/sec

                    m_statusLabel->setText(QStringLiteral("%1/s").arg(formatSize(static_cast<qint64>(speed))));
                    m_statusLabel->setStyleSheet(QStringLiteral("QLabel { color: #78716c; }"));
                    m_statusLabel->show();
                    m_lastReceived = received;
                    m_speedTimer.restart();
                }
            }
        }
    }
}

void FileListItemWidget::setLocalSavePath(const QString& path) {
    m_fileInfo.localSavePath = path;
}

void FileListItemWidget::markAsDownloaded() {
    m_downloading = false;
    m_progressBar->hide();
    m_statusLabel->hide();

    // 恢复删除按钮
    m_deleteButton->setToolTip(tr("删除"));

    // 信息行追加已下载标记
    QString infoText = tr("%1 · 来自 %2 · 已下载")
                           .arg(formatSize(m_fileInfo.fileSize), m_fileInfo.deviceName);
    m_infoLabel->setText(infoText);
}

void FileListItemWidget::resetDownload() {
    m_downloading = false;
    m_progressBar->hide();
    m_progressBar->setValue(0);
    m_statusLabel->hide();
    m_speedTimer.invalidate();
    m_lastReceived = 0;

    // 恢复删除按钮
    m_deleteButton->setToolTip(tr("删除"));
    m_deleteButton->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            border: none;
            border-radius: 10px;
        }
        QPushButton:hover {
            background: rgba(220, 38, 38, 0.12);
        }
    )");

    // 恢复原始信息行
    QString infoText = tr("%1 · 来自 %2").arg(formatSize(m_fileInfo.fileSize), m_fileInfo.deviceName);
    m_infoLabel->setText(infoText);
}

void FileListItemWidget::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event);
    m_deleteButton->show();
}

void FileListItemWidget::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    if (!m_downloading) {
        m_deleteButton->hide();
    }
}

QString FileListItemWidget::formatSize(qint64 bytes) {
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;

    if (bytes >= GB) {
        return QString::number(bytes / static_cast<double>(GB), 'f', 2) + QStringLiteral(" GB");
    } else if (bytes >= MB) {
        return QString::number(bytes / static_cast<double>(MB), 'f', 1) + QStringLiteral(" MB");
    } else if (bytes >= KB) {
        return QString::number(bytes / static_cast<double>(KB), 'f', 0) + QStringLiteral(" KB");
    } else {
        return QString::number(bytes) + QStringLiteral(" B");
    }
}

// ==================== FileListWidget ====================

FileListWidget::FileListWidget(QWidget* parent)
    : QWidget(parent) {
    setupUI();
}

void FileListWidget::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 空状态图标
    m_emptyIconLabel = new QLabel(this);
    QIcon emptyIcon(QStringLiteral(":/icons/empty-list.svg"));
    m_emptyIconLabel->setPixmap(emptyIcon.pixmap(64, 64));
    m_emptyIconLabel->setAlignment(Qt::AlignCenter);
    m_emptyIconLabel->setVisible(false);

    // 空状态提示
    m_emptyLabel = new QLabel(tr("暂无远程文件"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral("QLabel { color: #a8a29e; padding: 4px; }"));
    QFont emptyFont = m_emptyLabel->font();
    emptyFont.setPointSize(12);
    m_emptyLabel->setFont(emptyFont);
    m_emptyLabel->setVisible(false);

    // 列表组件
    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setSpacing(4);
    m_listWidget->setMouseTracking(true);

    // 应用样式 - 毛玻璃卡片 + hover 显示删除按钮
    m_listWidget->setStyleSheet(R"(
        QListWidget {
            background: transparent;
            border: none;
            outline: none;
        }
        QListWidget::item {
            border-radius: 10px;
            margin: 3px 2px;
            padding: 4px;
            background: rgba(255,255,255,0.55);
            border: 1px solid rgba(168,162,158,0.15);
        }
        QListWidget::item:hover {
            background: rgba(251,191,36,0.08);
            border: 1px solid rgba(217,119,6,0.20);
        }
        QListWidget::item:selected {
            background: rgba(251,191,36,0.12);
            border: 1px solid rgba(217,119,6,0.30);
        }
    )");

    mainLayout->addWidget(m_listWidget);
    updateEmptyState();

    connect(m_listWidget, &QListWidget::itemDoubleClicked,
            this, &FileListWidget::onItemDoubleClicked);
}

void FileListWidget::addFile(const SharedFileInfo& file) {
    // 检查是否已存在
    if (m_items.contains(file.fileId)) {
        return;
    }

    // 同一设备重复分享同名同大小文件时，替换旧 fileId，避免点击到失效条目
    QStringList staleIds;
    for (auto it = m_fileInfos.cbegin(); it != m_fileInfos.cend(); ++it) {
        const SharedFileInfo& existing = it.value();
        if (existing.deviceId == file.deviceId
            && existing.fileName == file.fileName
            && existing.fileSize == file.fileSize
            && existing.fileId != file.fileId) {
            staleIds.append(it.key());
        }
    }
    for (const QString& staleId : staleIds) {
        qDebug() << "Replacing stale fileId:" << staleId << "with" << file.fileId
                 << "for file:" << file.fileName;
        removeFile(staleId, file.deviceId);
    }

    // 创建列表项
    auto* item = new QListWidgetItem();
    item->setSizeHint(QSize(0, 90));

    // 创建自定义 widget
    auto* widget = new FileListItemWidget(file, m_listWidget);
    connect(widget, &FileListItemWidget::deleteRequested,
            this, &FileListWidget::onDeleteRequested);
    connect(widget, &FileListItemWidget::cancelRequested,
            this, &FileListWidget::onCancelRequested);

    m_listWidget->addItem(item);
    m_listWidget->setItemWidget(item, widget);

    m_items[file.fileId] = item;
    m_fileInfos[file.fileId] = file;

    updateEmptyState();
}

void FileListWidget::removeFile(const QString& fileId, const QString& deviceId) {
    Q_UNUSED(deviceId);

    auto it = m_items.find(fileId);
    if (it != m_items.end()) {
        int row = m_listWidget->row(it.value());
        auto* item = m_listWidget->takeItem(row);
        delete item;

        m_items.erase(it);
        m_fileInfos.remove(fileId);

        updateEmptyState();
    }
}

void FileListWidget::clearRemoteFiles(const QString& deviceId) {
    // 收集需要删除的文件 ID
    QStringList toRemove;
    for (auto it = m_fileInfos.begin(); it != m_fileInfos.end(); ++it) {
        if (it.value().deviceId == deviceId) {
            toRemove.append(it.key());
        }
    }

    // 删除所有匹配的文件
    for (const QString& fileId : toRemove) {
        removeFile(fileId, deviceId);
    }
}

void FileListWidget::updateTransferProgress(const QString& fileId, qint64 received, qint64 total) {
    auto it = m_items.find(fileId);
    if (it != m_items.end()) {
        auto* item = it.value();
        auto* widget = qobject_cast<FileListItemWidget*>(m_listWidget->itemWidget(item));
        if (widget) {
            widget->updateProgress(received, total);
        }
    }
}

void FileListWidget::updateFileSavePath(const QString& fileId, const QString& savePath) {
    // 更新文件信息中的保存路径
    auto infoIt = m_fileInfos.find(fileId);
    if (infoIt != m_fileInfos.end()) {
        infoIt.value().localSavePath = savePath;
    }

    // 更新列表项 widget
    auto itemIt = m_items.find(fileId);
    if (itemIt != m_items.end()) {
        auto* widget = qobject_cast<FileListItemWidget*>(
            m_listWidget->itemWidget(itemIt.value()));
        if (widget) {
            widget->setLocalSavePath(savePath);
            widget->markAsDownloaded();
        }

        // 移到列表末尾
        moveItemToEnd(fileId);
    }
}

void FileListWidget::moveItemToEnd(const QString& fileId) {
    auto it = m_items.find(fileId);
    if (it == m_items.end()) return;

    auto* item = it.value();
    int row = m_listWidget->row(item);

    // 已经在末尾则不需要移动
    if (row == m_listWidget->count() - 1) return;

    // 取出并重新插入到末尾
    m_listWidget->takeItem(row);
    m_listWidget->insertItem(m_listWidget->count(), item);

    // 重新设置 widget（takeItem 会断开）
    auto* widget = qobject_cast<FileListItemWidget*>(
        m_listWidget->itemWidget(item));
    // widget 在 takeItem 后可能被删除，需要重新创建
    if (!widget) {
        auto infoIt = m_fileInfos.find(fileId);
        if (infoIt != m_fileInfos.end()) {
            widget = new FileListItemWidget(infoIt.value(), m_listWidget);
            connect(widget, &FileListItemWidget::deleteRequested,
                    this, &FileListWidget::onDeleteRequested);
            m_listWidget->setItemWidget(item, widget);
        }
    }
}

void FileListWidget::onItemDoubleClicked(QListWidgetItem* item) {
    auto* widget = qobject_cast<FileListItemWidget*>(m_listWidget->itemWidget(item));
    if (!widget) {
        return;
    }

    QString fileId = widget->fileId();
    auto it = m_fileInfos.find(fileId);
    if (it == m_fileInfos.end()) {
        return;
    }

    // 已下载过则直接打开 Finder 定位
    if (!it.value().localSavePath.isEmpty()) {
        emit fileDownloadRequested(it.value(), it.value().localSavePath);
        return;
    }

    // 直接下载到系统 Downloads 目录
    QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QDir().mkpath(downloadDir);
    QString savePath = downloadDir + QLatin1Char('/') + it.value().fileName;

    // 避免文件名冲突
    if (QFile::exists(savePath) || QDir(savePath).exists()) {
        savePath = downloadDir + QLatin1Char('/')
                   + it.value().fileId + QLatin1Char('_') + it.value().fileName;
    }

    emit fileDownloadRequested(it.value(), savePath);
}

void FileListWidget::onDeleteRequested(const QString& fileId) {
    emit fileDeleteRequested(fileId);
}

void FileListWidget::onCancelRequested(const QString& fileId) {
    emit fileCancelRequested(fileId);
}

void FileListWidget::resetDownload(const QString& fileId) {
    auto it = m_items.find(fileId);
    if (it != m_items.end()) {
        auto* widget = qobject_cast<FileListItemWidget*>(m_listWidget->itemWidget(it.value()));
        if (widget) {
            widget->resetDownload();
        }
    }
}

void FileListWidget::retranslateUi() {
    m_emptyLabel->setText(tr("暂无远程文件"));
}

void FileListWidget::updateEmptyState() {
    bool isEmpty = m_items.isEmpty();
    m_emptyLabel->setVisible(isEmpty);
    m_emptyIconLabel->setVisible(isEmpty);
    m_listWidget->setVisible(!isEmpty);

    auto* mainLayout = qobject_cast<QVBoxLayout*>(layout());
    if (!mainLayout) return;

    if (isEmpty) {
        mainLayout->removeWidget(m_listWidget);
        if (mainLayout->indexOf(m_emptyIconLabel) == -1) {
            mainLayout->insertWidget(0, m_emptyIconLabel, 0, Qt::AlignCenter);
        }
        if (mainLayout->indexOf(m_emptyLabel) == -1) {
            mainLayout->insertWidget(1, m_emptyLabel, 0, Qt::AlignCenter);
        }
    } else {
        mainLayout->removeWidget(m_emptyIconLabel);
        mainLayout->removeWidget(m_emptyLabel);
        if (mainLayout->indexOf(m_listWidget) == -1) {
            mainLayout->insertWidget(0, m_listWidget);
        }
    }
}
