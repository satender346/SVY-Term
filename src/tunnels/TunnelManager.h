#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace svy::tunnels {

struct TunnelProfile {
    QString id;
    QString name;
    QString gatewayHost;
    int gatewayPort = 22;
    QString localHost = "127.0.0.1";
    int localPort = 0;
    QString remoteHost;
    int remotePort = 0;
};

class TunnelManager : public QObject {
    Q_OBJECT

public:
    explicit TunnelManager(QObject* parent = nullptr);

    bool startTunnel(const TunnelProfile& profile);
    bool stopTunnel(const QString& id);
    QVector<TunnelProfile> activeTunnels() const;

signals:
    void tunnelStarted(const TunnelProfile& profile);
    void tunnelStopped(const QString& id);
    void errorOccurred(const QString& message);

private:
    QVector<TunnelProfile> m_active;
};

} // namespace svy::tunnels
