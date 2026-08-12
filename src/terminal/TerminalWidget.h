#pragma once

#include "terminal/TerminalPane.h"

namespace svy::terminal {

class TerminalWidget : public TerminalPane {
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget* parent = nullptr);

private:
    static QString resolveLoginShell();
};

} // namespace svy::terminal
