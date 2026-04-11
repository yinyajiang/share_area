#pragma once
#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QSlider>
#include <QWidgetAction>
#include <QLabel>

/**
 * @brief 系统托盘组件
 * 提供最小化到托盘和右键菜单功能
 */
class SystemTray : public QObject {
    Q_OBJECT

public:
    explicit SystemTray(QObject* parent = nullptr);
    void show();
    void hide();
    void setVisible(bool visible);
    bool isVisible() const;
    void showMessage(const QString& title, const QString& message);
    void setToolTip(const QString& tooltip);
    void updateLanguageChecked(const QString& lang);
    void retranslateUi();

signals:
    void showWindowRequested();
    void changeLanguageRequested(const QString& lang);
    void changeCodeRequested();
    void opacityChanged(int opacity);
    void quitRequested();

private:
    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu* m_contextMenu = nullptr;
    QAction* m_showAction = nullptr;
    QMenu* m_languageMenu = nullptr;
    QActionGroup* m_langGroup = nullptr;
    QAction* m_chineseAction = nullptr;
    QAction* m_englishAction = nullptr;
    QAction* m_changeCodeAction = nullptr;
    QMenu* m_opacityMenu = nullptr;
    QSlider* m_opacitySlider = nullptr;
    QAction* m_quitAction = nullptr;

    void setupMenu();
};
