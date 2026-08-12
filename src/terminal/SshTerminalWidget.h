#pragma once

#include "core/SessionTypes.h"
#include "terminal/TerminalPane.h"

namespace svy::terminal {

class SshTerminalWidget : public TerminalPane {
    Q_OBJECT

public:
    explicit SshTerminalWidget(const svy::core::SessionProfile& profile, QWidget* parent = nullptr);

    const svy::core::SessionProfile& profile() const { return m_profile; }

private:
    svy::core::SessionProfile m_profile;
};

} // namespace svy::terminal
