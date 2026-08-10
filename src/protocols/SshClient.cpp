#include "protocols/SshClient.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QByteArray>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QString>

#include "protocols/CredentialCache.h"

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

QString shellEscapeSingleQuoted(const QString& value) {
    QString escaped = value;
    escaped.replace("'", "'\\''");
    return QString("'%1'").arg(escaped);
}

QString buildProxyCommand(const svy::core::SessionProfile& profile) {
    if (!profile.useProxy || profile.proxyHost.trimmed().isEmpty() || profile.proxyPort <= 0) {
        return {};
    }

    const QString proxyEndpoint = QString("%1:%2").arg(profile.proxyHost.trimmed()).arg(profile.proxyPort);
    const QString proxyAuth = (!profile.proxyUsername.trimmed().isEmpty() && !profile.proxyPassword.isEmpty())
                                  ? QString(" --proxy-auth %1")
                                        .arg(shellEscapeSingleQuoted(
                                            QString("%1:%2").arg(profile.proxyUsername.trimmed(), profile.proxyPassword)))
                                  : QString();

    // Use ncat if available, otherwise fallback to nc. Both forms keep %h/%p for destination expansion.
    const QString command = QString(
        "sh -c \"if command -v ncat >/dev/null 2>&1; then "
        "exec ncat --proxy %1 --proxy-type socks5%2 %%h %%p; "
        "else exec nc -X 5 -x %1 %%h %%p; fi\"")
                                .arg(proxyEndpoint, proxyAuth);
    return command;
}

bool ensureKnownHost(ssh_session session, const svy::core::SessionProfile& profile) {
    const int state = ssh_session_is_known_server(session);
    if (state == SSH_KNOWN_HOSTS_OK) {
        return true;
    }

    if (state == SSH_KNOWN_HOSTS_UNKNOWN || state == SSH_KNOWN_HOSTS_NOT_FOUND) {
        const auto answer = QMessageBox::question(
            nullptr,
            "SSH Host Verification",
            QString("Trust host key for %1:%2?").arg(profile.host).arg(profile.port),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return false;
        }
        return ssh_session_update_known_hosts(session) == SSH_OK;
    }

    return false;
}

QString buildSudoCommandWithPassword(const QString& command, const QString& password) {
    QString escaped = password;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    escaped.replace("$", "\\$");
    escaped.replace("`", "\\`");
    const QString body = command.mid(4).trimmed();
    return QString("printf \"%1\\n\" | sudo -S -p '' %2").arg(escaped, body);
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

    const QString proxyCommand = buildProxyCommand(profile);
    if (!proxyCommand.isEmpty()) {
        const QByteArray proxyCommandBytes = proxyCommand.toUtf8();
        ssh_options_set(m_session, SSH_OPTIONS_PROXYCOMMAND, proxyCommandBytes.constData());
    }

    if (ssh_connect(m_session) != SSH_OK) {
        emit errorOccurred(sshError(m_session, "SSH connect failed"));
        ssh_free(m_session);
        m_session = nullptr;
        return false;
    }

    if (!ensureKnownHost(m_session, profile)) {
        emit errorOccurred("Host key verification failed");
        ssh_disconnect(m_session);
        ssh_free(m_session);
        m_session = nullptr;
        return false;
    }

    QString effectivePassword = profile.password;
    if (effectivePassword.isEmpty()) {
        effectivePassword = svy::protocols::CredentialCache::getPassword(profile.username, profile.host, profile.port);
    }

    int authResult = ssh_userauth_publickey_auto(m_session, nullptr, nullptr);
    if (authResult != SSH_AUTH_SUCCESS && !effectivePassword.isEmpty()) {
        const QByteArray pass = effectivePassword.toUtf8();
        authResult = ssh_userauth_password(m_session, nullptr, pass.constData());
    }
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
            effectivePassword = password;
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
    if (!effectivePassword.isEmpty()) {
        m_current.password = effectivePassword;
        svy::protocols::CredentialCache::setPassword(profile.username, profile.host, profile.port, effectivePassword);
    }

    m_shellChannel = ssh_channel_new(m_session);
    if (m_shellChannel == nullptr) {
        emit errorOccurred("Failed to create SSH shell channel");
        ssh_disconnect(m_session);
        ssh_free(m_session);
        m_session = nullptr;
        return false;
    }

    if (ssh_channel_open_session(m_shellChannel) != SSH_OK) {
        emit errorOccurred(sshError(m_session, "Failed to open SSH shell channel"));
        ssh_channel_free(m_shellChannel);
        m_shellChannel = nullptr;
        ssh_disconnect(m_session);
        ssh_free(m_session);
        m_session = nullptr;
        return false;
    }

    if (ssh_channel_request_pty_size(m_shellChannel, "xterm-256color", 140, 40) != SSH_OK) {
        emit errorOccurred(sshError(m_session, "Failed to request PTY"));
        ssh_channel_close(m_shellChannel);
        ssh_channel_free(m_shellChannel);
        m_shellChannel = nullptr;
        ssh_disconnect(m_session);
        ssh_free(m_session);
        m_session = nullptr;
        return false;
    }

    if (ssh_channel_request_shell(m_shellChannel) != SSH_OK) {
        emit errorOccurred(sshError(m_session, "Failed to start remote shell"));
        ssh_channel_close(m_shellChannel);
        ssh_channel_free(m_shellChannel);
        m_shellChannel = nullptr;
        ssh_disconnect(m_session);
        ssh_free(m_session);
        m_session = nullptr;
        return false;
    }

    ssh_channel_set_blocking(m_shellChannel, 0);
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

    if (m_shellChannel == nullptr) {
        emit errorOccurred("SSH shell channel is not available");
        return false;
    }

    QString effectiveCommand = command;
    if (command.startsWith("sudo ")) {
        QString sudoPassword = m_current.password;
        if (sudoPassword.isEmpty()) {
            sudoPassword = svy::protocols::CredentialCache::getPassword(
                m_current.username, m_current.host, m_current.port);
        }
        if (sudoPassword.isEmpty()) {
            bool ok = false;
            sudoPassword = QInputDialog::getText(
                nullptr,
                "Sudo Password",
                QString("Sudo password for %1@%2").arg(m_current.username, m_current.host),
                QLineEdit::Password,
                QString(),
                &ok);
            if (!ok || sudoPassword.isEmpty()) {
                emit errorOccurred("Sudo command cancelled: password required");
                return false;
            }
            svy::protocols::CredentialCache::setPassword(
                m_current.username, m_current.host, m_current.port, sudoPassword);
            m_current.password = sudoPassword;
        }

        effectiveCommand = buildSudoCommandWithPassword(command, sudoPassword);
    }

    const QByteArray cmd = (effectiveCommand + "\n").toUtf8();
    if (ssh_channel_write(m_shellChannel, cmd.constData(), static_cast<uint32_t>(cmd.size())) == SSH_ERROR) {
        emit errorOccurred(sshError(m_session, "Failed to write to SSH shell"));
        return false;
    }

    QByteArray stdoutData;
    QByteArray stderrData;
    char buffer[4096];

    int idleLoops = 0;
    const int maxIdleLoops = 8;
    while (idleLoops < maxIdleLoops && !ssh_channel_is_eof(m_shellChannel)) {
        bool gotData = false;

        int nread = ssh_channel_read_nonblocking(m_shellChannel, buffer, sizeof(buffer), 0);
        if (nread == SSH_ERROR) {
            emit errorOccurred(sshError(m_session, "Error reading SSH stdout"));
            break;
        }
        if (nread > 0) {
            stdoutData.append(buffer, nread);
            gotData = true;
        }

        nread = ssh_channel_read_nonblocking(m_shellChannel, buffer, sizeof(buffer), 1);
        if (nread == SSH_ERROR) {
            emit errorOccurred(sshError(m_session, "Error reading SSH stderr"));
            break;
        }
        if (nread > 0) {
            stderrData.append(buffer, nread);
            gotData = true;
        }

        if (gotData) {
            idleLoops = 0;
        } else {
            ++idleLoops;
        }

        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    if (!stdoutData.isEmpty()) {
        emit outputReceived(QString::fromUtf8(stdoutData));
    }
    if (!stderrData.isEmpty()) {
        emit outputReceived(QString::fromUtf8(stderrData));
    }

    return true;
#endif
}

void SshClient::disconnectSession() {
#if SVYTERM_HAS_LIBSSH
    if (m_shellChannel != nullptr) {
        ssh_channel_send_eof(m_shellChannel);
        ssh_channel_close(m_shellChannel);
        ssh_channel_free(m_shellChannel);
        m_shellChannel = nullptr;
    }
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
