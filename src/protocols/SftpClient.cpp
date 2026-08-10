#include "protocols/SftpClient.h"

#include <QByteArray>
#include <QFile>
#include <QInputDialog>
#include <QLineEdit>
#include <QString>

#if SVYTERM_HAS_LIBSSH
#include <fcntl.h>
#include <sys/stat.h>

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

    const QString command = QString(
        "sh -c \"if command -v ncat >/dev/null 2>&1; then "
        "exec ncat --proxy %1 --proxy-type socks5%2 %%h %%p; "
        "else exec nc -X 5 -x %1 %%h %%p; fi\"")
                                .arg(proxyEndpoint, proxyAuth);
    return command;
}

} // namespace
#endif

namespace svy::protocols {

SftpClient::SftpClient(QObject* parent)
    : QObject(parent) {}

SftpClient::~SftpClient() {
    disconnectSession();
}

bool SftpClient::connectSession(const svy::core::SessionProfile& profile) {
    if (profile.host.trimmed().isEmpty()) {
        emit errorOccurred("SFTP host is required");
        return false;
    }

    disconnectSession();

#if !SVYTERM_HAS_LIBSSH
    m_connected = true;
    m_current = profile;
    emit info(QString("SFTP connected to %1:%2 (fallback mode, no libssh)").arg(profile.host).arg(profile.port));
    return true;
#else

    m_sshSession = ssh_new();
    if (m_sshSession == nullptr) {
        emit errorOccurred("Failed to allocate SSH session");
        return false;
    }

    const QByteArray host = profile.host.toUtf8();
    const QByteArray user = profile.username.toUtf8();
    const QByteArray keyPath = profile.privateKeyPath.toUtf8();
    unsigned int port = static_cast<unsigned int>(profile.port <= 0 ? 22 : profile.port);

    ssh_options_set(m_sshSession, SSH_OPTIONS_HOST, host.constData());
    ssh_options_set(m_sshSession, SSH_OPTIONS_PORT, &port);
    if (!profile.username.trimmed().isEmpty()) {
        ssh_options_set(m_sshSession, SSH_OPTIONS_USER, user.constData());
    }
    if (!profile.privateKeyPath.trimmed().isEmpty()) {
        ssh_options_set(m_sshSession, SSH_OPTIONS_IDENTITY, keyPath.constData());
    }

    const QString proxyCommand = buildProxyCommand(profile);
    if (!proxyCommand.isEmpty()) {
        const QByteArray proxyCommandBytes = proxyCommand.toUtf8();
        ssh_options_set(m_sshSession, SSH_OPTIONS_PROXYCOMMAND, proxyCommandBytes.constData());
    }

    if (ssh_connect(m_sshSession) != SSH_OK) {
        emit errorOccurred(sshError(m_sshSession, "SFTP SSH connect failed"));
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        return false;
    }

    int authResult = ssh_userauth_publickey_auto(m_sshSession, nullptr, nullptr);
    if (authResult != SSH_AUTH_SUCCESS && !profile.password.isEmpty()) {
        const QByteArray pass = profile.password.toUtf8();
        authResult = ssh_userauth_password(m_sshSession, nullptr, pass.constData());
    }
    if (authResult != SSH_AUTH_SUCCESS) {
        bool ok = false;
        const QString password = QInputDialog::getText(
            nullptr,
            "SFTP Password",
            QString("Password for %1@%2").arg(profile.username, profile.host),
            QLineEdit::Password,
            QString(),
            &ok);

        if (ok && !password.isEmpty()) {
            const QByteArray pass = password.toUtf8();
            authResult = ssh_userauth_password(m_sshSession, nullptr, pass.constData());
        }

        if (authResult != SSH_AUTH_SUCCESS) {
            emit errorOccurred(sshError(m_sshSession, "SFTP authentication failed"));
            ssh_disconnect(m_sshSession);
            ssh_free(m_sshSession);
            m_sshSession = nullptr;
            return false;
        }
    }

    m_sftpSession = sftp_new(m_sshSession);
    if (m_sftpSession == nullptr) {
        emit errorOccurred(sshError(m_sshSession, "Failed to create SFTP session"));
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        return false;
    }

    if (sftp_init(m_sftpSession) != SSH_OK) {
        emit errorOccurred("Failed to initialize SFTP session");
        sftp_free(m_sftpSession);
        m_sftpSession = nullptr;
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        return false;
    }

    m_connected = true;
    m_current = profile;
    emit info(QString("SFTP connected to %1:%2").arg(profile.host).arg(profile.port));
    return true;
#endif
}

QStringList SftpClient::listDirectory(const QString& path) const {
    if (!m_connected) {
        return {};
    }

#if !SVYTERM_HAS_LIBSSH
    return {
        QString("%1/.ssh").arg(path),
        QString("%1/.config").arg(path),
        QString("%1/logs").arg(path)
    };
#else

    const QByteArray remotePath = path.isEmpty() ? QByteArray(".") : path.toUtf8();
    sftp_dir dir = sftp_opendir(m_sftpSession, remotePath.constData());
    if (dir == nullptr) {
        return {};
    }

    QStringList entries;
    while (true) {
        sftp_attributes attrs = sftp_readdir(m_sftpSession, dir);
        if (attrs == nullptr) {
            break;
        }

        const QString name = QString::fromUtf8(attrs->name ? attrs->name : "");
        if (name != "." && name != "..") {
            entries.push_back(name);
        }
        sftp_attributes_free(attrs);
    }

    sftp_closedir(dir);
    return entries;
#endif
}

bool SftpClient::upload(const QString& localPath, const QString& remotePath) {
    if (!m_connected) {
        emit errorOccurred("SFTP not connected");
        return false;
    }

#if !SVYTERM_HAS_LIBSSH
    emit info(QString("Upload %1 -> %2 (fallback mode)").arg(localPath, remotePath));
    return true;
#else

    QFile local(localPath);
    if (!local.open(QIODevice::ReadOnly)) {
        emit errorOccurred(QString("Failed to open local file: %1").arg(localPath));
        return false;
    }

    const QByteArray remotePathBytes = remotePath.toUtf8();
    sftp_file remote = sftp_open(
        m_sftpSession,
        remotePathBytes.constData(),
        O_WRONLY | O_CREAT | O_TRUNC,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (remote == nullptr) {
        emit errorOccurred(QString("Failed to open remote file for upload: %1").arg(remotePath));
        return false;
    }

    while (!local.atEnd()) {
        const QByteArray chunk = local.read(32768);
        if (chunk.isEmpty() && local.error() != QFileDevice::NoError) {
            emit errorOccurred(QString("Read error from local file: %1").arg(localPath));
            sftp_close(remote);
            return false;
        }

        if (!chunk.isEmpty()) {
            const int written = sftp_write(remote, chunk.constData(), static_cast<size_t>(chunk.size()));
            if (written < 0 || written != chunk.size()) {
                emit errorOccurred(QString("Write error to remote file: %1").arg(remotePath));
                sftp_close(remote);
                return false;
            }
        }
    }

    sftp_close(remote);

    emit info(QString("Upload %1 -> %2").arg(localPath, remotePath));
    return true;
#endif
}

bool SftpClient::download(const QString& remotePath, const QString& localPath) {
    if (!m_connected) {
        emit errorOccurred("SFTP not connected");
        return false;
    }

#if !SVYTERM_HAS_LIBSSH
    emit info(QString("Download %1 -> %2 (fallback mode)").arg(remotePath, localPath));
    return true;
#else

    QFile local(localPath);
    if (!local.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit errorOccurred(QString("Failed to open local file for download: %1").arg(localPath));
        return false;
    }

    const QByteArray remotePathBytes = remotePath.toUtf8();
    sftp_file remote = sftp_open(m_sftpSession, remotePathBytes.constData(), O_RDONLY, 0);
    if (remote == nullptr) {
        emit errorOccurred(QString("Failed to open remote file for download: %1").arg(remotePath));
        return false;
    }

    char buffer[32768];
    while (true) {
        const int nread = sftp_read(remote, buffer, sizeof(buffer));
        if (nread < 0) {
            emit errorOccurred(QString("Read error from remote file: %1").arg(remotePath));
            sftp_close(remote);
            return false;
        }
        if (nread == 0) {
            break;
        }

        const qint64 written = local.write(buffer, nread);
        if (written != nread) {
            emit errorOccurred(QString("Write error to local file: %1").arg(localPath));
            sftp_close(remote);
            return false;
        }
    }

    sftp_close(remote);

    emit info(QString("Download %1 -> %2").arg(remotePath, localPath));
    return true;
#endif
}

void SftpClient::disconnectSession() {
    if (m_connected) {
        emit info("SFTP disconnected");
    }

#if SVYTERM_HAS_LIBSSH
    if (m_sftpSession != nullptr) {
        sftp_free(m_sftpSession);
        m_sftpSession = nullptr;
    }

    if (m_sshSession != nullptr) {
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
    }
#endif

    m_connected = false;
}

} // namespace svy::protocols
