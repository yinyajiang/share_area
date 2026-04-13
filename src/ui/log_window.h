#pragma once
#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

class LogWindow : public QWidget {
    Q_OBJECT

public:
    explicit LogWindow(QWidget* parent = nullptr);
    ~LogWindow();

    void appendLog(const QString& message);

private:
    QPlainTextEdit* m_textEdit = nullptr;
    QPushButton* m_clearButton = nullptr;
    QPushButton* m_copyButton = nullptr;
};

// 全局日志窗口指针，供消息处理器使用
extern LogWindow* g_logWindow;
