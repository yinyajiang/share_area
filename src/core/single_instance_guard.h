#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QLocalServer;

class SingleInstanceGuard : public QObject {
    Q_OBJECT

  public:
    explicit SingleInstanceGuard(const QString &serverName,
                                 QObject *parent = nullptr);
    ~SingleInstanceGuard() override;

    bool tryAcquire();
    bool notifyPrimaryInstance(const QByteArray &message = "show",
                               int timeoutMs = 1000) const;
    QString errorString() const;

  signals:
    void showRequested();

  private:
    void handleNewConnection();
    bool listen();

    QString m_serverName;
    QLocalServer *m_server = nullptr;
    QString m_errorString;
    bool m_acquired = false;
};
