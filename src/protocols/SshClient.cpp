#include "protocols/SshClient.h"

#include <QByteArray>
#include <QInputDialog>
#include <QLineEdit>

#if SVYTERM_HAS_LIBSSH
namespace {

QString sshError(ssh_session session, const QString& fallback) {
    if (session == nullptr) {
        return fallback;
    }
    const char* err = ssh_get_error(session);
    if (err == nullptr || *err == '\0') {
        return fallback;
    }
    return QString::fromUtf8(err);
}

} // namespace
#endif

namespace svy::protocols {

SshClient::SshClient(QObject* parent)
    : QObject(parent) {}

SshClient::~SshClient() {
    disconnectSession();
}

bool SshClient::connectSession(const svy::core::SessionProfile& profile) {
    if (profile.type != svy::core::SessionType::SSH) {
        emit errorOccurred("Invalid session type for SSH client");
        return false;
    }

    if (profile.host.trimmed().isEmpty()) {
        emit errorOccurred("SSH host is required");
        return false;
    }

    disconnectSession();

#if !SVYTERM_HAS_LIBSSH
    m_current = profile;
    m_connected = true;
    emit outputReceived(QString("Connected to %1@%2:%3 (fallback mode, no libssh)")
                            .arg(profile.username, profile.host)
                            .arg(profile.port));
    return true;
#else

    m_session = ssh_new();
    if (m_session == nullptr) {
        emit errorOccurred("Failed to allocate SSH session");
        return false;
    }

    const QByteArray host = profile.host.toUtf8();
    const QByteArray user = profile.username.toUtf8();
    const QByteArray keyPath = profile.privateKeyPath.toUtf8();
    unsigned int port = static_cast<unsigned int>(profile.port <= 0 ? 22 : profile.port);

    ssh_options_set(m_session, SSH_OPTIONS_HOST, host.constData());
    ssh_options_set(m_session, SSH_OPTIONS_PORT, &port);
    if (!profile.username.trimmed().isEmpty()) {
        ssh_options_set(m_session, SSH_OPTIONS_USER, user.constData());
    }
    if (!profile.privateKeyPath.trimmed().isEmpty()) {
        ssh_options_set(m_session, SSH_OPTIONS_IDENTITY, keyPath.constData());
    }

    if (ssh_connect(m_session) != SSH_OK) {
        emit errorOccurred(sshError(m_session, "SSH connect failed"));
        ssh_free(m_session);
        m_session = nullptr;
        return false;
    }

    int authResult = ssh_userauth_publickey_auto(m_session, nullptr, nullptr);
    if (authResult != SSH_AUTH_SUCCESS) {
        bool ok = false;
        const QString password = QInputDialog::getText(
            nullptr,
            "SSH Password",
            QString("Password for %1@%2").arg(profile.username, profile.host),
            QLineEdit::Password,
            QString(),
            &ok);

        if (ok && !password.isEmpty()) {
            const QByteArray pass = password.toUtf8();
            authResult = ssh_userauth_password(m_session, nullptr, pass.constData());
        }

        if (authResult != SSH_AUTH_SUCCESS) {
            emit errorOccurred(sshError(m_session, "SSH authentication failed"));
            ssh_disconnect(m_session);
            ssh_free(m_session);
            m_session = nullptr;
            return false;
        }
    }

    m_current = profile;
    m_connected = true;
    emit outputReceived(QString("Connected to %1@%2:%3")
                            .arg(profile.username, profile.host)
                            .arg(profile.port));
    return true;
#endif
}

bool SshClient::execute(const QString& command) {
    #if !SVYTERM_HAS_LIBSSH
        emit outputReceived(QString("[ssh-fallback %1] %2").arg(m_current.host, command));
        return true;
    #else
    if (!m_connected) {
        emit errorOccurred("SSH client is not connected");
        return false;
    }

    ssh_channel channel = ssh_channel_new(m_session);
    if (channel == nullptr) {
        emit errorOccurred("Failed to create SSH channel");
        return false;
    }

    if (ssh_channel_open_session(channel) != SSH_OK) {
        emit errorOccurred(sshError(m_session, "Failed to open SSH channel"));
        ssh_channel_free(channel);
        return false;
    }

    const QByteArray cmd = command.toUtf8();
    if (ssh_channel_request_exec(channel, cmd.constData()) != SSH_OK) {
        emit errorOccurred(sshError(m_session, "Failed to execute remote command"));
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return false;
    }

    QByteArray stdoutData;
    QByteArray stderrData;
    char buffer[4096];

    int nread = 0;
    while ((nread = ssh_channel_read(channel, buffer, sizeof(buffer), 0)) > 0) {
        stdoutData.append(buffer, nread);
    }

    while ((nread = ssh_channel_read(channel, buffer, sizeof(buffer), 1)) > 0) {
        stderrData.append(buffer, nread);
    }

    if (!stdoutData.isEmpty()) {
        emit outputReceived(QString::fromUtf8(stdoutData));
    }
    if (!stderrData.isEmpty()) {
        emit outputReceived(QString::fromUtf8(stderrData));
    }

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    return true;
#endif
}

void SshClient::disconnectSession() {
    if (m_connected) {
        emit outputReceived(QString("Disconnected from %1").arg(m_current.host));
    }
#if SVYTERM_HAS_LIBSSH
    if (m_session != nullptr) {
        ssh_disconnect(m_session);
        ssh_free(m_session);
        m_session = nullptr;
    }
#endif
    m_connected = false;
}

bool SshClient::isConnected() const {
    return m_connected;
}

} // namespace svy::protocols
