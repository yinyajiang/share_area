#include "app_icon.h"
#include <QIcon>

QIcon createAppIcon()
{
    return QIcon(QStringLiteral(":/icons/icon.svg"));
}

QIcon createTrayIcon()
{
    QIcon icon(QStringLiteral(":/icons/tray-icon.svg"));
    // macOS menu bar template icon（自适应明暗模式）
    icon.setIsMask(true);
    return icon;
}
