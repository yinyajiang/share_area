#pragma once
#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMap>
#include <QProgressBar>
#include <QElapsedTimer>
#include "core/shared_file.h"

/**
 * @brief 自定义列表项组件，显示文件信息
 */
class FileListItemWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileListItemWidget(const SharedFileInfo& fileInfo, QWidget* parent = nullptr);

    void updateProgress(qint64 received, qint64 total);
    void setLocalSavePath(const QString& path);
    void markAsDownloaded();
    QString fileId() const { return m_fileInfo.fileId; }
    QString deviceId() const { return m_fileInfo.deviceId; }
    bool isDownloaded() const { return !m_fileInfo.localSavePath.isEmpty(); }

signals:
    void deleteRequested(const QString& fileId);

private:
    SharedFileInfo m_fileInfo;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_infoLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_deleteButton = nullptr;

    // 速度计算相关
    QElapsedTimer m_speedTimer;
    qint64 m_lastReceived = 0;

    void setupUI();
    QString formatSize(qint64 bytes);
};

/**
 * @brief 远程文件列表组件
 * 显示其他设备分享的文件
 */
class FileListWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileListWidget(QWidget* parent = nullptr);
    void retranslateUi();

    void addFile(const SharedFileInfo& file);
    void removeFile(const QString& fileId, const QString& deviceId = QString());
    void clearRemoteFiles(const QString& deviceId);
    void updateTransferProgress(const QString& fileId, qint64 received, qint64 total);
    void updateFileSavePath(const QString& fileId, const QString& savePath);

signals:
    void fileDownloadRequested(const SharedFileInfo& file, const QString& savePath);
    void fileDeleteRequested(const QString& fileId);

private slots:
    void onItemDoubleClicked(QListWidgetItem* item);
    void onDeleteRequested(const QString& fileId);

private:
    QListWidget* m_listWidget = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QLabel* m_emptyIconLabel = nullptr;
    QMap<QString, QListWidgetItem*> m_items;  // fileId -> item
    QMap<QString, SharedFileInfo> m_fileInfos;

    void setupUI();
    void updateEmptyState();
    void moveItemToEnd(const QString& fileId);
};
