#include "app_icon.h"
#include <QIcon>

QIcon createAppIcon()
{
    return QIcon(QStringLiteral(":/icons/icon.svg"));
}

QIcon createTrayIcon()
{
#ifdef Q_OS_MACOS
    QIcon icon(QStringLiteral(":/icons/tray-icon.svg"));
    // macOS menu bar template icon（自适应明暗模式）
    icon.setIsMask(true);
    return icon;
#else
    // Windows / 其他系统使用程序图标
    return createAppIcon();
#endif
}
