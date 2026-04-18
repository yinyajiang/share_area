#pragma once
#include <QDialog>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>

class PeerAddressesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PeerAddressesDialog(QWidget* parent = nullptr);
    QStringList addresses() const;
    void setAddresses(const QStringList& addresses);
    void retranslateUi();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QPlainTextEdit* m_edit = nullptr;
    QPushButton* m_okButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QLabel* m_label = nullptr;

    bool m_dragging = false;
    QPoint m_dragStartPos;

    void setupUI();
};
