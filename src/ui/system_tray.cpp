#include "system_tray.h"
#include "app_icon.h"
#include "core/app_settings.h"
#include <QApplication>
#include <QDebug>
#include <QHBoxLayout>

SystemTray::SystemTray(QObject* parent)
    : QObject(parent) {
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(createTrayIcon());
    setupMenu();

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    emit showWindowRequested();
                }
            });
}

void SystemTray::setupMenu() {
    m_contextMenu = new QMenu();

    // 显示主窗口
    m_showAction = m_contextMenu->addAction(tr("显示主窗口"));
    connect(m_showAction, &QAction::triggered, this, &SystemTray::showWindowRequested);

    // 语言设置子菜单
    m_languageMenu = m_contextMenu->addMenu(tr("语言设置"));

    m_langGroup = new QActionGroup(this);
    m_chineseAction = m_languageMenu->addAction(QStringLiteral("中文"));
    m_chineseAction->setCheckable(true);
    m_langGroup->addAction(m_chineseAction);
    connect(m_chineseAction, &QAction::triggered, this, [this]() {
        emit changeLanguageRequested(QStringLiteral("zh_CN"));
    });

    m_englishAction = m_languageMenu->addAction(QStringLiteral("English"));
    m_englishAction->setCheckable(true);
    m_langGroup->addAction(m_englishAction);
    connect(m_englishAction, &QAction::triggered, this, [this]() {
        emit changeLanguageRequested(QStringLiteral("en"));
    });

    // 更换识别码
    m_changeCodeAction = m_contextMenu->addAction(tr("更换识别码"));
    connect(m_changeCodeAction, &QAction::triggered, this, &SystemTray::changeCodeRequested);

    // 窗口置顶
    m_alwaysOnTopAction = m_contextMenu->addAction(tr("窗口置顶"));
    m_alwaysOnTopAction->setCheckable(true);
    m_alwaysOnTopAction->setChecked(true);
    connect(m_alwaysOnTopAction, &QAction::triggered, this, [this]() {
        emit alwaysOnTopChanged(m_alwaysOnTopAction->isChecked());
    });

    // 透明度子菜单
    m_opacityMenu = m_contextMenu->addMenu(tr("透明度"));

    auto* opacityWidget = new QWidget(m_opacityMenu);
    auto* opacityLayout = new QHBoxLayout(opacityWidget);
    opacityLayout->setContentsMargins(12, 4, 12, 4);
    opacityLayout->setSpacing(8);

    m_opacitySlider = new QSlider(Qt::Horizontal, opacityWidget);
    m_opacitySlider->setRange(10, 100);
    m_opacitySlider->setValue(AppSettings::instance().opacity());
    m_opacitySlider->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal {"
        "   height: 4px; background: #d6d3d1; border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "   width: 14px; height: 14px; margin: -5px 0;"
        "   background: #d97706; border-radius: 7px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "   background: #d97706; border-radius: 2px;"
        "}"));
    opacityLayout->addWidget(m_opacitySlider, 1);

    auto* opacityAction = new QWidgetAction(m_opacityMenu);
    opacityAction->setDefaultWidget(opacityWidget);
    m_opacityMenu->addAction(opacityAction);

    connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int value) {
        emit opacityChanged(value);
    });

    // 退出
    m_contextMenu->addSeparator();
    m_quitAction = m_contextMenu->addAction(tr("退出"));
    m_quitAction->setShortcut(QStringLiteral("Ctrl+Q"));
    connect(m_quitAction, &QAction::triggered, this, &SystemTray::quitRequested);

    m_trayIcon->setContextMenu(m_contextMenu);
}

void SystemTray::show() {
    m_trayIcon->show();
}

void SystemTray::hide() {
    m_trayIcon->hide();
}

void SystemTray::setVisible(bool visible) {
    m_trayIcon->setVisible(visible);
}

bool SystemTray::isVisible() const {
    return m_trayIcon->isVisible();
}

void SystemTray::showMessage(const QString& title, const QString& message) {
    m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 3000);
}

void SystemTray::setToolTip(const QString& tooltip) {
    m_trayIcon->setToolTip(tooltip);
}

void SystemTray::updateLanguageChecked(const QString& lang) {
    if (lang == QStringLiteral("zh_CN")) {
        m_chineseAction->setChecked(true);
    } else {
        m_englishAction->setChecked(true);
    }
}

void SystemTray::setAlwaysOnTopChecked(bool on) {
    m_alwaysOnTopAction->setChecked(on);
}

void SystemTray::retranslateUi() {
    m_showAction->setText(tr("显示主窗口"));
    m_languageMenu->setTitle(tr("语言设置"));
    m_changeCodeAction->setText(tr("更换识别码"));
    m_alwaysOnTopAction->setText(tr("窗口置顶"));
    m_opacityMenu->setTitle(tr("透明度"));
    m_quitAction->setText(tr("退出"));
}
