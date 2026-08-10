#include "tunnels/TunnelManager.h"

#include <QProcess>
#include <QUuid>

namespace svy::tunnels {

TunnelManager::TunnelManager(QObject* parent)
    : QObject(parent) {}

bool TunnelManager::startTunnel(const TunnelProfile& profile) {
    if (profile.gatewayHost.isEmpty() || profile.remoteHost.isEmpty() || profile.localPort <= 0 || profile.remotePort <= 0) {
        emit errorOccurred("Gateway host and remote host are required");
        return false;
    }

    TunnelProfile started = profile;
    if (started.id.isEmpty()) {
        started.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    const QString destination = QString("%1:%2:%3")
                                    .arg(started.localHost)
                                    .arg(started.localPort)
                                    .arg(QString("%1:%2").arg(started.remoteHost).arg(started.remotePort));
    const QString gateway = started.gatewayUser.isEmpty()
                                ? started.gatewayHost
                                : QString("%1@%2").arg(started.gatewayUser, started.gatewayHost);

    QStringList args;
    args << "-N"
         << "-p" << QString::number(started.gatewayPort)
         << "-L" << destination;
    if (!started.privateKeyPath.isEmpty()) {
        args << "-i" << started.privateKeyPath;
    }
    args << gateway;

    auto* process = new QProcess(this);
    process->setProgram("ssh");
    process->setArguments(args);

    connect(process, &QProcess::errorOccurred, this, [this, started](QProcess::ProcessError) {
        emit errorOccurred(QString("Tunnel '%1' process error").arg(started.name.isEmpty() ? started.id : started.name));
    });
    connect(process, &QProcess::finished, this, [this, started](int, QProcess::ExitStatus) {
        stopTunnel(started.id);
    });

    process->start();
    if (!process->waitForStarted(3000)) {
        emit errorOccurred("Failed to start tunnel process");
        process->deleteLater();
        return false;
    }

    m_processes.insert(started.id, process);
    m_active.push_back(started);
    emit tunnelStarted(started);
    return true;
}

bool TunnelManager::stopTunnel(const QString& id) {
    if (m_processes.contains(id) && !m_processes[id].isNull()) {
        auto* process = m_processes[id].data();
        process->kill();
        process->waitForFinished(1000);
        process->deleteLater();
        m_processes.remove(id);
    }

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
