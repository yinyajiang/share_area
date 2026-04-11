#pragma once
#include <QIcon>

// 生成应用图标（彩色，用于 Dock/任务栏/窗口标题栏）
QIcon createAppIcon();

// 生成托盘图标（单色模板，macOS 自适应明暗模式）
QIcon createTrayIcon();
