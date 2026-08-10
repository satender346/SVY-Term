#pragma once

#include <QObject>
#include <QStringList>

#if SVYTERM_HAS_LIBSSH
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#endif

#include "core/SessionTypes.h"

namespace svy::protocols {

class SftpClient : public QObject {
    Q_OBJECT

public:
    explicit SftpClient(QObject* parent = nullptr);
    ~SftpClient() override;

    bool connectSession(const svy::core::SessionProfile& profile);
    bool isConnected() const;
    QStringList listDirectory(const QString& path) const;
    bool upload(const QString& localPath, const QString& remotePath);
    bool download(const QString& remotePath, const QString& localPath);
    void disconnectSession();

signals:
    void info(const QString& message);
    void errorOccurred(const QString& message);

private:
    bool m_connected = false;
    svy::core::SessionProfile m_current;
#if SVYTERM_HAS_LIBSSH
    ssh_session m_sshSession = nullptr;
    sftp_session m_sftpSession = nullptr;
#endif
};

} // namespace svy::protocols
