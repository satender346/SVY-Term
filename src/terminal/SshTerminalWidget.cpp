#include "terminal/SshTerminalWidget.h"

#include <QStringList>

namespace svy::terminal {

SshTerminalWidget::SshTerminalWidget(const svy::core::SessionProfile& profile, QWidget* parent)
    : TerminalPane(parent),
      m_profile(profile) {
    QStringList arguments;
    // -t forces remote PTY allocation so password prompts and full-screen apps run in this terminal.
    arguments << "-t";

    if (m_profile.port > 0 && m_profile.port != 22) {
        arguments << "-p" << QString::number(m_profile.port);
    }
    if (!m_profile.privateKeyPath.trimmed().isEmpty()) {
        arguments << "-i" << m_profile.privateKeyPath.trimmed();
    }

    const QString host = m_profile.host.trimmed();
    const QString user = m_profile.username.trimmed();
    arguments << (user.isEmpty() ? host : QString("%1@%2").arg(user, host));

    startProcess("ssh", arguments);
}

} // namespace svy::terminal
