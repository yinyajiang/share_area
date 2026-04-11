#include "file_list_widget.h"
#include <QFileDialog>
#include <QHBoxLayout>
#include <QStyle>
#include <QFileInfo>
#include <QStandardPaths>
#include <QIcon>

// ==================== FileListItemWidget ====================

FileListItemWidget::FileListItemWidget(const SharedFileInfo& fileInfo, QWidget* parent)
    : QWidget(parent), m_fileInfo(fileInfo) {
    setupUI();
}

void FileListItemWidget::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(6);
    layout->setContentsMargins(12, 10, 12, 10);

    // 文件名
    m_nameLabel = new QLabel(m_fileInfo.fileName, this);
    QFont nameFont = m_nameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(12);
    m_nameLabel->setFont(nameFont);
    m_nameLabel->setStyleSheet(QStringLiteral("QLabel { color: #292524; }"));
    layout->addWidget(m_nameLabel);

    // 信息行（大小 + 来源）
    m_infoLabel = new QLabel(this);
    QFont infoFont = m_infoLabel->font();
    infoFont.setPointSize(10);
    m_infoLabel->setFont(infoFont);
    m_infoLabel->setStyleSheet(QStringLiteral("QLabel { color: #78716c; }"));
    QString infoText = tr("%1 · 来自 %2").arg(formatSize(m_fileInfo.fileSize), m_fileInfo.deviceName);
    m_infoLabel->setText(infoText);
    layout->addWidget(m_infoLabel);

    // 进度条（初始隐藏）
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat(tr("%p%"));
    m_progressBar->setMaximumHeight(8);
    m_progressBar->hide();
    layout->addWidget(m_progressBar);

    // 状态标签（初始隐藏）
    m_statusLabel = new QLabel(this);
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(10);
    m_statusLabel->setFont(statusFont);
    m_statusLabel->hide();
    layout->addWidget(m_statusLabel);
}

void FileListItemWidget::updateProgress(qint64 received, qint64 total) {
    if (total > 0) {
        int percent = static_cast<int>((received * 100) / total);
        m_progressBar->setValue(percent);
        m_progressBar->show();

        if (received >= total) {
            m_statusLabel->setText(tr("下载完成"));
            m_statusLabel->setStyleSheet(QStringLiteral("QLabel { color: #27ae60; }"));
            m_statusLabel->show();
        } else {
            m_statusLabel->hide();
        }
    }
    update();
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

    // 应用样式 - 毛玻璃卡片
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

    // 创建列表项
    auto* item = new QListWidgetItem();
    item->setSizeHint(QSize(0, 90));

    // 创建自定义 widget
    auto* widget = new FileListItemWidget(file, m_listWidget);

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

    // 弹出保存对话框
    QString fileName = it.value().fileName;
    QString suggestedPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    suggestedPath += QLatin1Char('/') + fileName;

    QString savePath = QFileDialog::getSaveFileName(
        this,
        tr("保存文件"),
        suggestedPath,
        QString()
    );

    if (!savePath.isEmpty()) {
        emit fileDownloadRequested(it.value(), savePath);
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
