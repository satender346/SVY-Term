#include "tunnels/TunnelManager.h"

#include <QProcess>
#include <QRegularExpression>
#include <QUuid>

#include <memory>

namespace svy::tunnels {

TunnelManager::TunnelManager(QObject* parent)
    : QObject(parent) {}

bool TunnelManager::startTunnel(const TunnelProfile& profile) {
    if (profile.gatewayHost.isEmpty()) {
        emit errorOccurred("Gateway host is required");
        return false;
    }

    const QString mode = profile.mode.trimmed().isEmpty() ? "local" : profile.mode.trimmed().toLower();
    if (mode != "local" && mode != "remote" && mode != "dynamic") {
        emit errorOccurred("Tunnel mode must be local, remote, or dynamic");
        return false;
    }

    if (profile.localPort <= 0) {
        emit errorOccurred("Local port is required");
        return false;
    }

    if (mode != "dynamic" && (profile.remoteHost.isEmpty() || profile.remotePort <= 0)) {
        emit errorOccurred("Remote host and remote port are required");
        return false;
    }

    TunnelProfile started = profile;
    if (started.id.isEmpty()) {
        started.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    if (m_processes.contains(started.id)) {
        stopTunnel(started.id);
    }

    const QString gateway = started.gatewayUser.isEmpty()
                                ? started.gatewayHost
                                : QString("%1@%2").arg(started.gatewayUser, started.gatewayHost);

    QStringList args;
    args << "-N"
            << "-p" << QString::number(started.gatewayPort)
            << "-o" << "ExitOnForwardFailure=yes"
            << "-o" << "BatchMode=no"
            << "-o" << "NumberOfPasswordPrompts=1";

    if (mode == "dynamic") {
        const QString dynamicDestination = QString("%1:%2")
                                               .arg(started.localHost)
                                               .arg(started.localPort);
        args << "-D" << dynamicDestination;
    } else {
        const QString destination = QString("%1:%2:%3")
                                        .arg(started.localHost)
                                        .arg(started.localPort)
                                        .arg(QString("%1:%2").arg(started.remoteHost).arg(started.remotePort));
        args << (mode == "remote" ? "-R" : "-L") << destination;
    }

    if (!started.privateKeyPath.isEmpty()) {
        args << "-i" << started.privateKeyPath;
    }
    args << gateway;

    auto* process = new QProcess(this);
    process->setProgram("ssh");
    process->setArguments(args);
    process->setProcessChannelMode(QProcess::SeparateChannels);

    struct TunnelProcessState {
        bool passwordSent = false;
        QString stderrText;
    };
    auto state = std::make_shared<TunnelProcessState>();

    connect(process, &QProcess::readyReadStandardError, this, [process, started, state]() {
        state->stderrText += QString::fromLocal8Bit(process->readAllStandardError());
        if (state->passwordSent || started.gatewayPassword.isEmpty()) {
            return;
        }

        static const QRegularExpression passwordPrompt(QStringLiteral("password\\s*:\\s*$"),
                                                       QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
        if (!state->stderrText.contains(passwordPrompt)) {
            return;
        }

        process->write(started.gatewayPassword.toUtf8());
        process->write("\n");
        state->passwordSent = true;
    });

    connect(process, &QProcess::errorOccurred, this, [this, started](QProcess::ProcessError) {
        emit errorOccurred(QString("Tunnel '%1' process error").arg(started.name.isEmpty() ? started.id : started.name));
    });

    connect(process, &QProcess::finished, this, [this, started, state](int exitCode, QProcess::ExitStatus) {
        const bool expectedStop = m_stoppingIds.contains(started.id);
        m_stoppingIds.remove(started.id);
        if (!expectedStop && exitCode != 0) {
            const QString detail = state->stderrText.trimmed();
            const QString message = detail.isEmpty()
                                        ? QString("Tunnel '%1' stopped unexpectedly").arg(started.name.isEmpty() ? started.id : started.name)
                                        : QString("Tunnel '%1' failed: %2").arg(started.name.isEmpty() ? started.id : started.name, detail);
            emit errorOccurred(message);
        }
        stopTunnel(started.id);
    });

    process->start();
    if (!process->waitForStarted(3000)) {
        emit errorOccurred("Failed to start tunnel process");
        process->deleteLater();
        return false;
    }

    if (process->waitForFinished(300)) {
        const QString detail = state->stderrText.trimmed();
        emit errorOccurred(detail.isEmpty() ? "Tunnel process exited before forwarding became active" : detail);
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
        m_stoppingIds.insert(id);
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
