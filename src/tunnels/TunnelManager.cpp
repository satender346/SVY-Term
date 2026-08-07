#include "tunnels/TunnelManager.h"

namespace svy::tunnels {

TunnelManager::TunnelManager(QObject* parent)
    : QObject(parent) {}

bool TunnelManager::startTunnel(const TunnelProfile& profile) {
    if (profile.gatewayHost.isEmpty() || profile.remoteHost.isEmpty()) {
        emit errorOccurred("Gateway host and remote host are required");
        return false;
    }

    m_active.push_back(profile);
    emit tunnelStarted(profile);
    return true;
}

bool TunnelManager::stopTunnel(const QString& id) {
    for (int i = 0; i < m_active.size(); ++i) {
        if (m_active[i].id == id) {
            m_active.removeAt(i);
            emit tunnelStopped(id);
            return true;
        }
    }
    return false;
}

QVector<TunnelProfile> TunnelManager::activeTunnels() const {
    return m_active;
}

} // namespace svy::tunnels
