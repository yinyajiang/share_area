#pragma once
#include <QWidget>
#include <QLabel>
#include <QUrl>
#include <QVBoxLayout>

/**
 * @brief 文件拖放区域组件
 * 支持拖拽文件到区域内进行分享
 */
class DropAreaWidget : public QWidget {
    Q_OBJECT

public:
    explicit DropAreaWidget(QWidget* parent = nullptr);
    void retranslateUi();

signals:
    void filesDropped(const QList<QUrl>& urls);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_dragActive = false;
    QLabel* m_hintLabel = nullptr;
    QLabel* m_iconLabel = nullptr;

    void setupUI();
    void updateDragState(bool active);
};
