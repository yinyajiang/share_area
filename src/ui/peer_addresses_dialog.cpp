#include "peer_addresses_dialog.h"
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

PeerAddressesDialog::PeerAddressesDialog(QWidget* parent) : QDialog(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setupUI();
}

void PeerAddressesDialog::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect rect = this->rect();
    qreal r = 12.0;

    QPainterPath clip;
    clip.addRoundedRect(rect, r, r);
    p.setClipPath(clip);

    QLinearGradient grad(QPointF(0, 0),
                         QPointF(rect.width() * 0.6, rect.height()));
    grad.setColorAt(0.0, QColor("#fafaf9"));
    grad.setColorAt(0.5, QColor("#f5f5f4"));
    grad.setColorAt(1.0, QColor("#f0ede8"));
    p.fillRect(rect, grad);

    QRadialGradient glow1(QPointF(rect.width() * 0.1, -20), 200);
    glow1.setColorAt(0.0, QColor(251, 191, 36, 35));
    glow1.setColorAt(1.0, QColor(251, 191, 36, 0));
    p.fillRect(rect, glow1);

    QRadialGradient glow2(QPointF(rect.width() * 0.9, rect.height() + 20), 220);
    glow2.setColorAt(0.0, QColor(148, 163, 184, 25));
    glow2.setColorAt(1.0, QColor(148, 163, 184, 0));
    p.fillRect(rect, glow2);
}

void PeerAddressesDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStartPos =
            event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void PeerAddressesDialog::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragStartPos);
        event->accept();
    }
}

void PeerAddressesDialog::mouseReleaseEvent(QMouseEvent* event) {
    m_dragging = false;
    event->accept();
}

void PeerAddressesDialog::setupUI() {
    setWindowTitle(QStringLiteral("ShareArea"));
    setFixedSize(360, 300);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(24, 20, 24, 24);

    // 标题
    m_label = new QLabel(tr("指定目标地址（每行一个 IP）"), this);
    m_label->setStyleSheet(QStringLiteral(
        "QLabel { color: #292524; font-size: 13px; font-weight: 600; }"));
    mainLayout->addWidget(m_label);

    // 输入区
    m_edit = new QPlainTextEdit(this);
    m_edit->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        "   border: 1.5px solid rgba(168,162,158,0.35);"
        "   border-radius: 8px;"
        "   padding: 8px;"
        "   font-size: 13px;"
        "   background: rgba(255,255,255,0.70);"
        "   color: #292524;"
        "}"
        "QPlainTextEdit:focus {"
        "   border-color: #d97706;"
        "   background: rgba(255,255,255,0.90);"
        "}"));
    mainLayout->addWidget(m_edit, 1);

    // 按钮
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_cancelButton = new QPushButton(tr("取消"), this);
    m_cancelButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "   background: transparent; color: #78716c; border: 1.5px solid rgba(168,162,158,0.35);"
        "   border-radius: 6px; padding: 8px 20px; font-size: 13px;"
        "}"
        "QPushButton:hover { background: rgba(0,0,0,0.04); }"));
    buttonLayout->addWidget(m_cancelButton);

    m_okButton = new QPushButton(tr("确定"), this);
    m_okButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "   background: #d97706; color: white; border: none;"
        "   border-radius: 6px; padding: 8px 20px; font-size: 13px;"
        "}"
        "QPushButton:hover { background: #b45309; }"));
    buttonLayout->addWidget(m_okButton);

    mainLayout->addLayout(buttonLayout);

    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
}

QStringList PeerAddressesDialog::addresses() const {
    QStringList result;
    for (const QString& line : m_edit->toPlainText().split('\n')) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
        }
    }
    return result;
}

void PeerAddressesDialog::setAddresses(const QStringList& addresses) {
    m_edit->setPlainText(addresses.join('\n'));
}

void PeerAddressesDialog::retranslateUi() {
    m_label->setText(tr("指定目标地址（每行一个 IP）"));
    m_cancelButton->setText(tr("取消"));
    m_okButton->setText(tr("确定"));
}
