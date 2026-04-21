#include "auto_start.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>

#ifdef Q_OS_MACOS
#include <QStandardPaths>
#elif defined(Q_OS_WIN)
#include <QSettings>
#endif

static const char *kLaunchAgentId = "com.sharearea.app";

#ifdef Q_OS_MACOS
static QString plistPath() {
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
           + QStringLiteral("/Library/LaunchAgents/%1.plist").arg(kLaunchAgentId);
}

// 从可执行文件路径推导 .app bundle 路径
static QString bundlePath() {
    QString execPath = QCoreApplication::applicationFilePath();
    int idx = execPath.indexOf(QStringLiteral(".app/"));
    if (idx >= 0) {
        return execPath.left(idx + 4);  // include ".app"
    }
    return execPath;
}
#endif

bool AutoStart::isEnabled() {
#ifdef Q_OS_MACOS
    return QFile::exists(plistPath());
#elif defined(Q_OS_WIN)
    QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                       QSettings::NativeFormat);
    return settings.contains(QStringLiteral("ShareArea"));
#else
    return false;
#endif
}

void AutoStart::setEnabled(bool enabled) {
#ifdef Q_OS_MACOS
    QString path = plistPath();

    // 确保 LaunchAgents 目录存在
    QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    if (enabled) {
        QString app = bundlePath();

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return;
        }
        file.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\""
                   " \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                   "<plist version=\"1.0\">\n"
                   "<dict>\n"
                   "    <key>Label</key>\n"
                   "    <string>");
        file.write(kLaunchAgentId);
        file.write("</string>\n"
                   "    <key>ProgramArguments</key>\n"
                   "    <array>\n"
                   "        <string>/usr/bin/open</string>\n"
                   "        <string>");
        file.write(app.toUtf8());
        file.write("</string>\n"
                   "        <string>--args</string>\n"
                   "        <string>--hidden</string>\n"
                   "    </array>\n"
                   "    <key>RunAtLoad</key>\n"
                   "    <true/>\n"
                   "</dict>\n"
                   "</plist>\n");
        file.close();

        // 立即加载，下次登录也会自动加载
        QProcess::startDetached(QStringLiteral("launchctl"),
                                {QStringLiteral("load"), path});
    } else {
        // 先卸载再删除
        QProcess::startDetached(QStringLiteral("launchctl"),
                                {QStringLiteral("unload"), path});
        QFile::remove(path);
    }
#elif defined(Q_OS_WIN)
    QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                       QSettings::NativeFormat);
    if (enabled) {
        QString appPath = QCoreApplication::applicationFilePath();
        appPath.replace(QLatin1Char('/'), QLatin1Char('\\'));
        settings.setValue(QStringLiteral("ShareArea"),
                          QStringLiteral("\"%1\" --hidden").arg(appPath));
    } else {
        settings.remove(QStringLiteral("ShareArea"));
    }
#endif
}
