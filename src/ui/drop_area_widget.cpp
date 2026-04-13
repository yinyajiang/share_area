#include "drop_area_widget.h"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QPainterPath>
#include <QIcon>

DropAreaWidget::DropAreaWidget(QWidget* parent)
    : QWidget(parent) {
    setAcceptDrops(true);
    setMinimumHeight(140);
    setMaximumHeight(180);
    setupUI();
}

void DropAreaWidget::setupUI() {
    // 中心布局
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(8);

    // 拖拽区域图标（固定容器大小，图标在其中缩放不影响布局）
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(72, 72);
    QIcon dropIcon(QStringLiteral(":/icons/drop-area.svg"));
    m_iconLabel->setPixmap(dropIcon.pixmap(64, 64));
    m_iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_iconLabel, 0, Qt::AlignCenter);
    layout->addSpacing(-4);

    // 提示文字
    m_hintLabel = new QLabel(tr("拖拽文件到此处分享"), this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    QFont font = m_hintLabel->font();
    font.setPointSize(13);
    m_hintLabel->setFont(font);
    m_hintLabel->setStyleSheet(QStringLiteral("QLabel { color: #78716c; }"));
    layout->addWidget(m_hintLabel, 0, Qt::AlignCenter);

    // 副提示（双击分享剪贴板）
    m_subHintLabel = new QLabel(tr("双击分享剪贴板"), this);
    m_subHintLabel->setAlignment(Qt::AlignCenter);
    QFont subFont = m_subHintLabel->font();
    subFont.setPointSize(10);
    m_subHintLabel->setFont(subFont);
    m_subHintLabel->setStyleSheet(QStringLiteral("QLabel { color: rgba(120,113,108,0.45); }"));
    layout->addWidget(m_subHintLabel, 0, Qt::AlignCenter);
}

void DropAreaWidget::retranslateUi() {
    m_hintLabel->setText(tr("拖拽文件到此处分享"));
    m_subHintLabel->setText(tr("双击分享剪贴板"));
}

void DropAreaWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        updateDragState(true);
    }
}

void DropAreaWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void DropAreaWidget::dragLeaveEvent(QDragLeaveEvent* event) {
    Q_UNUSED(event);
    updateDragState(false);
}

void DropAreaWidget::dropEvent(QDropEvent* event) {
    updateDragState(false);

    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        // 过滤掉非文件 URL
        QList<QUrl> fileUrls;
        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                fileUrls.append(url);
            }
        }

        if (!fileUrls.isEmpty()) {
            emit filesDropped(fileUrls);
        }
        event->acceptProposedAction();
    }
}

void DropAreaWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect rect = this->rect().adjusted(2, 2, -2, -2);
    QPainterPath path;
    path.addRoundedRect(rect, 12, 12);

    // 毛玻璃半透明背景
    QColor bg;
    if (m_dragActive) {
        bg = QColor(251, 191, 36, 22);          // 暖琥珀
    } else {
        bg = QColor(255, 255, 255, 100);         // 半透明白
    }
    painter.fillPath(path, bg);

    // 柔和边框
    QPen pen;
    pen.setWidth(2);
    pen.setColor(m_dragActive
        ? QColor(217, 119, 6, 140)               // 琥珀色边框
        : QColor(168, 162, 158, 90));             // 暖灰边框
    pen.setStyle(m_dragActive ? Qt::SolidLine : Qt::DashLine);
    painter.setPen(pen);
    painter.drawPath(path);
}

void DropAreaWidget::updateDragState(bool active) {
    if (m_dragActive != active) {
        m_dragActive = active;
        update();  // 触发重绘
    }
}

void DropAreaWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit shareClipboardRequested();
    }
}

void DropAreaWidget::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event);
    QIcon dropIcon(QStringLiteral(":/icons/drop-area.svg"));
    m_iconLabel->setPixmap(dropIcon.pixmap(72, 72));
    setCursor(Qt::PointingHandCursor);
}

void DropAreaWidget::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    QIcon dropIcon(QStringLiteral(":/icons/drop-area.svg"));
    m_iconLabel->setPixmap(dropIcon.pixmap(64, 64));
    unsetCursor();
}
