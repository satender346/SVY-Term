#pragma once

#include <QObject>
#include <QString>

#if SVYTERM_HAS_LIBSSH
#include <libssh/libssh.h>
#include <libssh/callbacks.h>
#endif

#include "core/SessionTypes.h"

namespace svy::protocols {

class SshClient : public QObject {
    Q_OBJECT

public:
    explicit SshClient(QObject* parent = nullptr);
    ~SshClient() override;

    bool connectSession(const svy::core::SessionProfile& profile);
    bool execute(const QString& command);
    void disconnectSession();
    bool isConnected() const;

signals:
    void outputReceived(const QString& output);
    void errorOccurred(const QString& message);

private:
    bool m_connected = false;
    svy::core::SessionProfile m_current;
#if SVYTERM_HAS_LIBSSH
    ssh_session m_session = nullptr;
    ssh_channel m_shellChannel = nullptr;
#endif
};

} // namespace svy::protocols
