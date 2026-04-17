#include "system_tray.h"
#include "app_icon.h"
#include "core/app_settings.h"
#include <QApplication>
#include <QDebug>
#include <QHBoxLayout>

SystemTray::SystemTray(QObject *parent) : QObject(parent) {
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(createTrayIcon());
    setupMenu();

    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    emit showWindowRequested();
                }
            });
}

void SystemTray::setupMenu() {
    m_contextMenu = new QMenu();

    // 显示主窗口
    m_showAction = m_contextMenu->addAction(tr("显示主窗口"));
    connect(m_showAction, &QAction::triggered, this,
            &SystemTray::showWindowRequested);

    m_contextMenu->addSeparator();

    // 外观设置子菜单
    m_appearanceMenu = m_contextMenu->addMenu(tr("外观设置"));

    // 窗口置顶
    m_alwaysOnTopAction = m_appearanceMenu->addAction(tr("窗口置顶"));
    m_alwaysOnTopAction->setCheckable(true);
    m_alwaysOnTopAction->setChecked(true);
    connect(m_alwaysOnTopAction, &QAction::triggered, this, [this]() {
        emit alwaysOnTopChanged(m_alwaysOnTopAction->isChecked());
    });

    // 透明度子菜单
    m_opacityMenu = m_appearanceMenu->addMenu(tr("透明度"));

    auto *opacityWidget = new QWidget(m_opacityMenu);
    auto *opacityLayout = new QHBoxLayout(opacityWidget);
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

    auto *opacityAction = new QWidgetAction(m_opacityMenu);
    opacityAction->setDefaultWidget(opacityWidget);
    m_opacityMenu->addAction(opacityAction);

    connect(m_opacitySlider, &QSlider::valueChanged, this,
            [this](int value) { emit opacityChanged(value); });

    // 传输设置子菜单
    m_transferMenu = m_contextMenu->addMenu(tr("传输设置"));

    // 下载路径
    m_downloadPathAction = m_transferMenu->addAction(tr("默认下载目录"));
    connect(m_downloadPathAction, &QAction::triggered, this,
            &SystemTray::changeDownloadPathRequested);

    // 自动移除已下载项子菜单
    m_autoDeleteMenu = m_transferMenu->addMenu(tr("自动移除已下载项"));
    m_autoDeleteGroup = new QActionGroup(this);

    struct Option { QString label; int seconds; };
    QList<Option> options = {
        { tr("手动删除"), 0 },
        { tr("5 秒"), 5 },
        { tr("30 秒"), 30 },
        { tr("1 分钟"), 60 },
        { tr("5 分钟"), 300 },
    };
    int currentSecs = AppSettings::instance().autoDeleteSeconds();
    for (const auto &opt : options) {
        QAction *act = m_autoDeleteMenu->addAction(opt.label);
        act->setCheckable(true);
        act->setData(opt.seconds);
        m_autoDeleteGroup->addAction(act);
        if (opt.seconds == currentSecs) {
            act->setChecked(true);
        }
        connect(act, &QAction::triggered, this, [this, act]() {
            emit autoDeleteChanged(act->data().toInt());
        });
    }

    // 通用设置子菜单
    m_generalMenu = m_contextMenu->addMenu(tr("通用设置"));

    // 语言设置子菜单
    m_languageMenu = m_generalMenu->addMenu(tr("语言设置"));

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
    connect(m_englishAction, &QAction::triggered, this,
            [this]() { emit changeLanguageRequested(QStringLiteral("en")); });

    // 更换识别码
    m_changeCodeAction = m_generalMenu->addAction(tr("更换识别码"));
    connect(m_changeCodeAction, &QAction::triggered, this,
            &SystemTray::changeCodeRequested);

    // 开机启动
    m_autoStartAction = m_generalMenu->addAction(tr("开机启动"));
    m_autoStartAction->setCheckable(true);
    m_autoStartAction->setChecked(AppSettings::instance().autoStart());
    connect(m_autoStartAction, &QAction::triggered, this, [this]() {
        emit autoStartChanged(m_autoStartAction->isChecked());
    });

    // 其它设置子菜单
    m_otherMenu = m_contextMenu->addMenu(tr("其它设置"));

    m_debugLogAction = m_otherMenu->addAction(tr("调试日志"));
    m_debugLogAction->setCheckable(true);
    m_debugLogAction->setChecked(false);
    connect(m_debugLogAction, &QAction::triggered, this, [this]() {
        emit debugLogToggled(m_debugLogAction->isChecked());
    });

    // 退出
    m_contextMenu->addSeparator();

    m_quitAction = m_contextMenu->addAction(tr("退出"));
    m_quitAction->setShortcut(QStringLiteral("Ctrl+Q"));
    connect(m_quitAction, &QAction::triggered, this,
            &SystemTray::quitRequested);

    m_trayIcon->setContextMenu(m_contextMenu);
}

void SystemTray::show() { m_trayIcon->show(); }

void SystemTray::hide() { m_trayIcon->hide(); }

void SystemTray::setVisible(bool visible) { m_trayIcon->setVisible(visible); }

bool SystemTray::isVisible() const { return m_trayIcon->isVisible(); }

void SystemTray::showMessage(const QString &title, const QString &message) {
    m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 3000);
}

void SystemTray::setToolTip(const QString &tooltip) {
    m_trayIcon->setToolTip(tooltip);
}

void SystemTray::updateLanguageChecked(const QString &lang) {
    if (lang == QStringLiteral("zh_CN")) {
        m_chineseAction->setChecked(true);
    } else {
        m_englishAction->setChecked(true);
    }
}

void SystemTray::setAlwaysOnTopChecked(bool on) {
    m_alwaysOnTopAction->setChecked(on);
}

void SystemTray::updateAutoDeleteChecked(int seconds) {
    for (QAction *act : m_autoDeleteGroup->actions()) {
        if (act->data().toInt() == seconds) {
            act->setChecked(true);
            return;
        }
    }
    // fallback: select manual (0)
    for (QAction *act : m_autoDeleteGroup->actions()) {
        if (act->data().toInt() == 0) {
            act->setChecked(true);
            return;
        }
    }
}

void SystemTray::retranslateUi() {
    m_showAction->setText(tr("显示主窗口"));
    m_appearanceMenu->setTitle(tr("外观设置"));
    m_transferMenu->setTitle(tr("传输设置"));
    m_generalMenu->setTitle(tr("通用设置"));
    m_otherMenu->setTitle(tr("其它设置"));
    m_languageMenu->setTitle(tr("语言设置"));
    m_changeCodeAction->setText(tr("更换识别码"));
    m_debugLogAction->setText(tr("调试日志"));
    m_alwaysOnTopAction->setText(tr("窗口置顶"));
    m_opacityMenu->setTitle(tr("透明度"));
    m_downloadPathAction->setText(tr("默认下载目录"));
    m_autoDeleteMenu->setTitle(tr("自动移除已下载项"));
    m_autoStartAction->setText(tr("开机启动"));
    // 更新自动移除已下载项选项文字
    struct { int secs; const char *key; } labels[] = {
        { 0, QT_TR_NOOP("手动删除") },
        { 5, QT_TR_NOOP("5 秒") },
        { 30, QT_TR_NOOP("30 秒") },
        { 60, QT_TR_NOOP("1 分钟") },
        { 300, QT_TR_NOOP("5 分钟") },
    };
    const auto actions = m_autoDeleteGroup->actions();
    for (int i = 0; i < actions.size() && i < 5; ++i) {
        actions[i]->setText(tr(labels[i].key));
    }
    m_quitAction->setText(tr("退出"));
}
