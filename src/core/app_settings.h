#pragma once
#include <QObject>
#include <QSettings>
#include <QString>

class AppSettings : public QObject {
    Q_OBJECT

  public:
    static AppSettings &instance();

    QString groupCode() const;
    void setGroupCode(const QString &code);
    bool hasGroupCode() const;

    QString language() const;
    void setLanguage(const QString &lang);

    QString deviceName() const;
    void setDeviceName(const QString &name);

    QString deviceId() const;

    int opacity() const;
    void setOpacity(int opacity); // 10-100

    bool alwaysOnTop() const;
    void setAlwaysOnTop(bool alwaysOnTop);

    QString downloadPath() const;
    void setDownloadPath(const QString &path);

    int autoDeleteSeconds() const;
    void setAutoDeleteSeconds(int seconds);

    bool autoStart() const;
    void setAutoStart(bool on);

    void save();
    void load();

  private:
    AppSettings();
    ~AppSettings() = default;

    QSettings m_settings;
    QString m_groupCode;
    QString m_language;
    QString m_deviceName;
    QString m_deviceId;
    int m_opacity = 92;
    bool m_alwaysOnTop = true;
    QString m_downloadPath;
    int m_autoDeleteSeconds = 5;
    bool m_autoStart = true;
};
