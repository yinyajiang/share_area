#include "app_settings.h"
#include <QSysInfo>
#include <QUuid>

AppSettings &AppSettings::instance() {
    static AppSettings instance;
    return instance;
}

AppSettings::AppSettings()
    : m_settings(QStringLiteral("ShareArea"), QStringLiteral("ShareArea")) {
    load();
}

QString AppSettings::groupCode() const { return m_groupCode; }

void AppSettings::setGroupCode(const QString &code) { m_groupCode = code; }

bool AppSettings::hasGroupCode() const { return !m_groupCode.isEmpty(); }

QString AppSettings::language() const { return m_language; }

void AppSettings::setLanguage(const QString &lang) { m_language = lang; }

QString AppSettings::deviceName() const { return m_deviceName; }

void AppSettings::setDeviceName(const QString &name) { m_deviceName = name; }

QString AppSettings::deviceId() const { return m_deviceId; }

int AppSettings::opacity() const { return m_opacity; }

void AppSettings::setOpacity(int opacity) {
    m_opacity = qBound(10, opacity, 100);
}

void AppSettings::save() {
    m_settings.setValue(QStringLiteral("groupCode"), m_groupCode);
    m_settings.setValue(QStringLiteral("language"), m_language);
    m_settings.setValue(QStringLiteral("deviceName"), m_deviceName);
    m_settings.setValue(QStringLiteral("deviceId"), m_deviceId);
    m_settings.setValue(QStringLiteral("opacity"), m_opacity);
}

void AppSettings::load() {
    m_groupCode = m_settings.value(QStringLiteral("groupCode")).toString();
    m_language = m_settings.value(QStringLiteral("language")).toString();

    m_deviceName = m_settings.value(QStringLiteral("deviceName")).toString();
    if (m_deviceName.isEmpty()) {
        m_deviceName = QSysInfo::machineHostName();
    }

    m_deviceId = m_settings.value(QStringLiteral("deviceId")).toString();
    if (m_deviceId.isEmpty()) {
        m_deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    m_opacity = m_settings.value(QStringLiteral("opacity"), 100).toInt();
    m_opacity = qBound(10, m_opacity, 100);
}
