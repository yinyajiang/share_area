#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

/**
 * @brief 首次运行时输入识别码的对话框
 */
class SetupDialog : public QDialog {
    Q_OBJECT

public:
    explicit SetupDialog(QWidget* parent = nullptr);
    QString groupCode() const;
    void retranslateUi();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QLineEdit* m_codeEdit = nullptr;
    QPushButton* m_okButton = nullptr;
    QPushButton* m_closeButton = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_hintLabel = nullptr;

    bool m_dragging = false;
    QPoint m_dragStartPos;

    void setupUI();
    void validateInput();
};
