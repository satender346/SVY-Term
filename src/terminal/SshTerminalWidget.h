#pragma once

#include <QWidget>

#include "core/SessionTypes.h"

class QLineEdit;
class QPlainTextEdit;

namespace svy::protocols {
class SshClient;
}

namespace svy::terminal {

class SshTerminalWidget : public QWidget {
    Q_OBJECT

public:
    explicit SshTerminalWidget(const svy::core::SessionProfile& profile, QWidget* parent = nullptr);
    void runCommand(const QString& command);

private slots:
    void onEnterPressed();
    void onOutputReceived(const QString& output);
    void onError(const QString& message);

private:
    void append(const QString& line);

    svy::core::SessionProfile m_profile;
    QPlainTextEdit* m_output;
    QLineEdit* m_input;
    svy::protocols::SshClient* m_client;
};

} // namespace svy::terminal
