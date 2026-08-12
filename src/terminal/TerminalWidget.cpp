#include "terminal/TerminalWidget.h"

#include <QFileInfo>
#include <QProcessEnvironment>

namespace svy::terminal {

TerminalWidget::TerminalWidget(QWidget* parent)
    : TerminalPane(parent) {
    const QString shell = resolveLoginShell();
    startProcess(shell, {"-l"});
}

QString TerminalWidget::resolveLoginShell() {
    const QString configured = QProcessEnvironment::systemEnvironment().value("SHELL");
    if (!configured.isEmpty() && QFileInfo::exists(configured)) {
        return configured;
    }
    if (QFileInfo::exists("/bin/zsh")) {
        return "/bin/zsh";
    }
    return "/bin/bash";
}

} // namespace svy::terminal
