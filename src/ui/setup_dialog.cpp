#include "setup_dialog.h"
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QMouseEvent>

SetupDialog::SetupDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setupUI();
}

void SetupDialog::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect rect = this->rect();
    qreal r = 12.0;

    QPainterPath clip;
    clip.addRoundedRect(rect, r, r);
    p.setClipPath(clip);

    // 和主窗口相同的暖白渐变背景
    QLinearGradient grad(QPointF(0, 0), QPointF(rect.width() * 0.6, rect.height()));
    grad.setColorAt(0.0, QColor("#fafaf9"));
    grad.setColorAt(0.5, QColor("#f5f5f4"));
    grad.setColorAt(1.0, QColor("#f0ede8"));
    p.fillRect(rect, grad);

    // 装饰光晕（左上，暖琥珀）
    QRadialGradient glow1(QPointF(rect.width() * 0.1, -20), 200);
    glow1.setColorAt(0.0, QColor(251, 191, 36, 35));
    glow1.setColorAt(1.0, QColor(251, 191, 36, 0));
    p.fillRect(rect, glow1);

    // 装饰光晕（右下，淡青）
    QRadialGradient glow2(QPointF(rect.width() * 0.9, rect.height() + 20), 220);
    glow2.setColorAt(0.0, QColor(148, 163, 184, 25));
    glow2.setColorAt(1.0, QColor(148, 163, 184, 0));
    p.fillRect(rect, glow2);
}

void SetupDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStartPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void SetupDialog::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragStartPos);
        event->accept();
    }
}

void SetupDialog::mouseReleaseEvent(QMouseEvent* event) {
    m_dragging = false;
    event->accept();
}

bool SetupDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_closeButton) {
        if (event->type() == QEvent::Enter) {
            m_closeButton->setIcon(QIcon(QStringLiteral(":/icons/close-hover.svg")));
        } else if (event->type() == QEvent::Leave) {
            m_closeButton->setIcon(QIcon(QStringLiteral(":/icons/close.svg")));
        }
    }
    return QDialog::eventFilter(watched, event);
}

void SetupDialog::setupUI() {
    setWindowTitle(QStringLiteral("ShareArea"));
    setFixedSize(400, 240);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(24, 20, 24, 24);

    // 顶部栏：Logo + 标题 + 关闭按钮
    auto* topBar = new QHBoxLayout();
    topBar->setSpacing(8);

    QIcon logoIcon(QStringLiteral(":/icons/logo.svg"));
    auto* logoLabel = new QLabel(this);
    logoLabel->setPixmap(logoIcon.pixmap(28, 28));
    topBar->addWidget(logoLabel);

    auto* titleLabel = new QLabel(QStringLiteral("ShareArea"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #292524; font-size: 14px; font-weight: 600; }"));
    topBar->addWidget(titleLabel);
    topBar->addStretch();

    m_closeButton = new QPushButton(this);
    m_closeButton->setFixedSize(28, 28);
    m_closeButton->setIcon(QIcon(QStringLiteral(":/icons/close.svg")));
    m_closeButton->setIconSize(QSize(14, 14));
    m_closeButton->setToolTip(tr("关闭"));
    m_closeButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:transparent; border:none; border-radius:14px; }"
        "QPushButton:hover { background:rgba(239,68,68,0.10); }"));
    m_closeButton->installEventFilter(this);
    topBar->addWidget(m_closeButton);
    mainLayout->addLayout(topBar);

    // 说明文字
    m_hintLabel = new QLabel(tr("请输入识别码以加入分享组"), this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #78716c; font-size: 12px; }"));
    mainLayout->addWidget(m_hintLabel);

    // 输入框
    m_codeEdit = new QLineEdit(this);
    m_codeEdit->setPlaceholderText(tr("输入 1-6 位识别码"));
    m_codeEdit->setMaxLength(6);
    m_codeEdit->setAlignment(Qt::AlignCenter);
    m_codeEdit->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "   border: 1.5px solid rgba(168,162,158,0.35);"
        "   border-radius: 8px;"
        "   padding: 10px;"
        "   font-size: 13px;"
        "   background: rgba(255,255,255,0.70);"
        "   color: #292524;"
        "}"
        "QLineEdit:focus {"
        "   border-color: #d97706;"
        "   background: rgba(255,255,255,0.90);"
        "}"));
    mainLayout->addWidget(m_codeEdit);

    // 确认按钮
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_okButton = new QPushButton(tr("确认"), this);
    m_okButton->setEnabled(false);
    m_okButton->setMinimumWidth(120);
    m_okButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "   background: #d97706; color: white; border: none;"
        "   border-radius: 6px; padding: 10px 24px; font-size: 13px;"
        "}"
        "QPushButton:hover { background: #b45309; }"
        "QPushButton:disabled { background: #d6d3d1; color: #a8a29e; }"));
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    // 信号连接
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_codeEdit, &QLineEdit::textChanged, this, &SetupDialog::validateInput);
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_codeEdit, &QLineEdit::returnPressed, this, [this]() {
        if (m_okButton->isEnabled()) accept();
    });

    m_codeEdit->setFocus();
}

QString SetupDialog::groupCode() const {
    return m_codeEdit->text().trimmed();
}

void SetupDialog::retranslateUi() {
    m_closeButton->setToolTip(tr("关闭"));
    m_hintLabel->setText(tr("请输入识别码以加入分享组"));
    m_codeEdit->setPlaceholderText(tr("输入 1-6 位识别码"));
    m_okButton->setText(tr("确认"));
}

void SetupDialog::validateInput() {
    QString text = m_codeEdit->text().trimmed();
    bool valid = !text.isEmpty();
    m_okButton->setEnabled(valid);
}
