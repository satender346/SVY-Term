#include "terminal/SshTerminalWidget.h"

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include "protocols/SshClient.h"

namespace svy::terminal {

SshTerminalWidget::SshTerminalWidget(const svy::core::SessionProfile& profile, QWidget* parent)
    : QWidget(parent),
      m_profile(profile),
      m_output(new QPlainTextEdit(this)),
      m_input(new QLineEdit(this)),
      m_client(new svy::protocols::SshClient(this)) {
    m_output->setReadOnly(true);
    m_input->setPlaceholderText("Enter remote command and press Return");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(m_output);
    layout->addWidget(m_input);
    setLayout(layout);

    connect(m_input, &QLineEdit::returnPressed, this, &SshTerminalWidget::onEnterPressed);
    connect(m_client, &svy::protocols::SshClient::outputReceived,
            this, &SshTerminalWidget::onOutputReceived);
    connect(m_client, &svy::protocols::SshClient::errorOccurred,
            this, &SshTerminalWidget::onError);

    append(QString("Connecting to %1@%2:%3 ...")
               .arg(m_profile.username, m_profile.host)
               .arg(m_profile.port));

    if (m_client->connectSession(m_profile)) {
        append("Connected.");
    }
}

void SshTerminalWidget::onEnterPressed() {
    const QString command = m_input->text().trimmed();
    if (command.isEmpty()) {
        return;
    }

    m_input->clear();
    append(QString("$ %1").arg(command));
    m_client->execute(command);
}

void SshTerminalWidget::onOutputReceived(const QString& output) {
    append(output);
}

void SshTerminalWidget::onError(const QString& message) {
    append(QString("[error] %1").arg(message));
}

void SshTerminalWidget::append(const QString& line) {
    if (!line.isEmpty()) {
        m_output->appendPlainText(line);
    }
}

} // namespace svy::terminal
