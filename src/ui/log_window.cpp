#include "log_window.h"
#include <QApplication>
#include <QClipboard>
#include <QScrollBar>
#include <QtMessageHandler>

LogWindow* g_logWindow = nullptr;

static QtMessageHandler originalHandler = nullptr;

static void logWindowMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    // 调用原始处理器（输出到控制台）
    if (originalHandler) {
        originalHandler(type, context, msg);
    }

    // 转发到日志窗口
    if (g_logWindow) {
        QString prefix;
        switch (type) {
        case QtDebugMsg:    prefix = QStringLiteral("[DEBUG] "); break;
        case QtWarningMsg:  prefix = QStringLiteral("[WARN] "); break;
        case QtCriticalMsg: prefix = QStringLiteral("[CRIT] "); break;
        case QtFatalMsg:    prefix = QStringLiteral("[FATAL] "); break;
        case QtInfoMsg:     prefix = QStringLiteral("[INFO] "); break;
        }
        g_logWindow->appendLog(prefix + msg);
    }
}

LogWindow::LogWindow(QWidget* parent)
    : QWidget(parent) {
    setWindowTitle(tr("调试日志"));
    resize(640, 480);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // 日志文本框
    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_textEdit->setStyleSheet(
        QStringLiteral("QPlainTextEdit {"
                        "  font-family: 'Menlo', 'Consolas', monospace;"
                        "  font-size: 12px;"
                        "  background: #1e1e1e;"
                        "  color: #d4d4d4;"
                        "  border: 1px solid #3c3c3c;"
                        "  border-radius: 6px;"
                        "  padding: 8px;"
                        "}"));
    layout->addWidget(m_textEdit, 1);

    // 按钮行
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_copyButton = new QPushButton(tr("复制全部"), this);
    m_copyButton->setStyleSheet(
        QStringLiteral("QPushButton {"
                        "  padding: 6px 16px;"
                        "  border-radius: 6px;"
                        "  background: #d97706;"
                        "  color: white;"
                        "  border: none;"
                        "}"
                        "QPushButton:hover { background: #b45309; }"));
    connect(m_copyButton, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_textEdit->toPlainText());
    });
    btnLayout->addWidget(m_copyButton);

    m_clearButton = new QPushButton(tr("清空"), this);
    m_clearButton->setStyleSheet(
        QStringLiteral("QPushButton {"
                        "  padding: 6px 16px;"
                        "  border-radius: 6px;"
                        "  background: #78716c;"
                        "  color: white;"
                        "  border: none;"
                        "}"
                        "QPushButton:hover { background: #57534e; }"));
    connect(m_clearButton, &QPushButton::clicked, m_textEdit, &QPlainTextEdit::clear);
    btnLayout->addWidget(m_clearButton);

    layout->addLayout(btnLayout);

    // 安装消息处理器
    g_logWindow = this;
    originalHandler = qInstallMessageHandler(logWindowMessageHandler);
}

LogWindow::~LogWindow() {
    // 恢复原始消息处理器
    qInstallMessageHandler(originalHandler);
    originalHandler = nullptr;
    g_logWindow = nullptr;
}

void LogWindow::appendLog(const QString& message) {
    m_textEdit->appendPlainText(message);
    // 自动滚动到底部
    QScrollBar* scroll = m_textEdit->verticalScrollBar();
    scroll->setValue(scroll->maximum());
}
