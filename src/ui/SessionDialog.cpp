#include "ui/SessionDialog.h"

#include <algorithm>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace svy::ui {

SessionDialog::SessionDialog(QWidget* parent)
    : QDialog(parent),
      m_name(new QLineEdit(this)),
      m_type(new QComboBox(this)),
      m_host(new QLineEdit(this)),
      m_port(new QSpinBox(this)),
      m_username(new QLineEdit(this)),
      m_authMethod(new QComboBox(this)),
      m_password(new QLineEdit(this)),
      m_rememberCredentials(new QCheckBox("Remember credentials (stored in Keychain)", this)),
      m_keyPath(new QLineEdit(this)),
      m_browseKeyPath(new QPushButton("Browse...", this)),
      m_useProxy(new QCheckBox("Use proxy", this)),
      m_proxyHost(new QLineEdit(this)),
      m_proxyPort(new QSpinBox(this)),
      m_proxyUsername(new QLineEdit(this)),
      m_proxyPassword(new QLineEdit(this)),
      m_tunnelMode(new QComboBox(this)),
      m_tunnelLocalPort(new QSpinBox(this)),
      m_tunnelRemoteHost(new QLineEdit(this)),
      m_tunnelRemotePort(new QSpinBox(this)),
      m_startupCommand(new QLineEdit(this)) {
    setWindowTitle("Session");
    resize(700, 560);

    m_type->addItem("Local", "local");
    m_type->addItem("SSH", "ssh");

    m_authMethod->addItem("Password", "password");
    m_authMethod->addItem("Private key", "key");
    m_authMethod->addItem("SSH agent", "agent");

    m_password->setEchoMode(QLineEdit::Password);
    m_proxyPassword->setEchoMode(QLineEdit::Password);

    m_port->setRange(1, 65535);
    m_port->setValue(22);
    m_proxyPort->setRange(1, 65535);
    m_proxyPort->setValue(8080);
    m_tunnelLocalPort->setRange(1, 65535);
    m_tunnelRemotePort->setRange(1, 65535);
    m_tunnelRemotePort->setValue(22);

    m_tunnelMode->addItem("None", "none");
    m_tunnelMode->addItem("Local port forwarding", "local");
    m_tunnelMode->addItem("Remote port forwarding", "remote");
    m_tunnelMode->addItem("Dynamic (SOCKS)", "dynamic");

    auto* keyPathRow = new QWidget(this);
    auto* keyPathRowLayout = new QHBoxLayout(keyPathRow);
    keyPathRowLayout->setContentsMargins(0, 0, 0, 0);
    keyPathRowLayout->addWidget(m_keyPath);
    keyPathRowLayout->addWidget(m_browseKeyPath);

    auto* form = new QFormLayout();
    form->addRow("Name", m_name);
    form->addRow("Type", m_type);
    form->addRow("Host", m_host);
    form->addRow("Port", m_port);
    form->addRow("Username", m_username);
    form->addRow("Authentication", m_authMethod);
    form->addRow("Password", m_password);
    form->addRow("", m_rememberCredentials);
    form->addRow("Private key path", keyPathRow);
    form->addRow("Proxy", m_useProxy);
    form->addRow("Proxy host", m_proxyHost);
    form->addRow("Proxy port", m_proxyPort);
    form->addRow("Proxy username", m_proxyUsername);
    form->addRow("Proxy password", m_proxyPassword);
    form->addRow("Tunnel mode", m_tunnelMode);
    form->addRow("Tunnel local port", m_tunnelLocalPort);
    form->addRow("Tunnel remote host", m_tunnelRemoteHost);
    form->addRow("Tunnel remote port", m_tunnelRemotePort);
    form->addRow("Startup command", m_startupCommand);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(m_type, &QComboBox::currentIndexChanged, this, [this](int) { onTypeChanged(); });
    connect(m_authMethod, &QComboBox::currentIndexChanged, this, [this](int) { onAuthMethodChanged(); });
    connect(m_browseKeyPath, &QPushButton::clicked, this, &SessionDialog::onBrowseKeyPath);
    connect(m_host, &QLineEdit::textChanged, this, &SessionDialog::onHostChanged);
    connect(m_useProxy, &QCheckBox::toggled, this, &SessionDialog::onProxyToggled);
    connect(m_tunnelMode, &QComboBox::currentIndexChanged, this, [this](int) { onTunnelModeChanged(); });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    onTypeChanged();
    onProxyToggled(false);
    onTunnelModeChanged();
}

void SessionDialog::setProfile(const svy::core::SessionProfile& p) {
    m_name->setText(p.name);
    m_type->setCurrentIndex(p.type == svy::core::SessionType::SSH ? 1 : 0);
    m_host->setText(p.host);
    m_port->setValue(p.port > 0 ? p.port : 22);
    m_username->setText(p.username);

    const int authIdx = std::max(0, m_authMethod->findData(p.authMethod));
    m_authMethod->setCurrentIndex(authIdx);

    // Never pre-fill the password field from the profile — it should be empty (Keychain-backed).
    m_password->clear();
    m_rememberCredentials->setChecked(p.rememberCredentials);
    m_keyPath->setText(p.privateKeyPath);
    m_useProxy->setChecked(p.useProxy);
    m_proxyHost->setText(p.proxyHost);
    m_proxyPort->setValue(p.proxyPort > 0 ? p.proxyPort : 8080);
    m_proxyUsername->setText(p.proxyUsername);
    m_proxyPassword->clear();
    const int tunnelIndex = std::max(0, m_tunnelMode->findData(p.tunnelMode));
    m_tunnelMode->setCurrentIndex(tunnelIndex);
    m_tunnelLocalPort->setValue(p.tunnelLocalPort > 0 ? p.tunnelLocalPort : 8080);
    m_tunnelRemoteHost->setText(p.tunnelRemoteHost);
    m_tunnelRemotePort->setValue(p.tunnelRemotePort > 0 ? p.tunnelRemotePort : 22);
    m_startupCommand->setText(p.startupCommand);
    onTypeChanged();
    onProxyToggled(m_useProxy->isChecked());
    onTunnelModeChanged();
}

svy::core::SessionProfile SessionDialog::profile() const {
    svy::core::SessionProfile p;
    p.name = m_name->text().trimmed();
    p.type = m_type->currentData().toString() == "ssh"
                 ? svy::core::SessionType::SSH
                 : svy::core::SessionType::Local;
    p.host = m_host->text().trimmed();
    p.port = m_port->value();
    p.username = m_username->text().trimmed();
    p.authMethod = m_authMethod->currentData().toString();
    // password is intentionally NOT copied into the profile — use enteredPassword() separately.
    p.rememberCredentials = m_rememberCredentials->isChecked();
    p.privateKeyPath = m_keyPath->text().trimmed();
    p.useProxy = m_useProxy->isChecked();
    p.proxyHost = m_proxyHost->text().trimmed();
    p.proxyPort = m_proxyPort->value();
    p.proxyUsername = m_proxyUsername->text().trimmed();
    p.tunnelMode = m_tunnelMode->currentData().toString();
    p.tunnelLocalPort = m_tunnelLocalPort->value();
    p.tunnelRemoteHost = m_tunnelRemoteHost->text().trimmed();
    p.tunnelRemotePort = m_tunnelRemotePort->value();
    p.startupCommand = m_startupCommand->text().trimmed();
    return p;
}

QString SessionDialog::enteredPassword() const {
    return m_password->text();
}

bool SessionDialog::rememberCredentials() const {
    return m_rememberCredentials->isChecked();
}

void SessionDialog::onTypeChanged() {
    const bool isSsh = m_type->currentData().toString() == "ssh";
    m_host->setEnabled(isSsh);
    m_port->setEnabled(isSsh);
    m_username->setEnabled(isSsh);
    m_authMethod->setEnabled(isSsh);
    m_useProxy->setEnabled(isSsh);
    m_tunnelMode->setEnabled(isSsh);
    onAuthMethodChanged();
    onProxyToggled(isSsh && m_useProxy->isChecked());
    onTunnelModeChanged();
}

void SessionDialog::onAuthMethodChanged() {
    const bool isSsh = m_type->currentData().toString() == "ssh";
    const QString method = m_authMethod->currentData().toString();
    const bool isPassword = method == "password";
    const bool isKey = method == "key";
    m_password->setEnabled(isSsh && isPassword);
    m_rememberCredentials->setEnabled(isSsh && isPassword);
    m_keyPath->setEnabled(isSsh && isKey);
    m_browseKeyPath->setEnabled(isSsh && isKey);
}

void SessionDialog::onBrowseKeyPath() {
    const QString path = QFileDialog::getOpenFileName(this, "Select private key", QString());
    if (!path.isEmpty()) {
        m_keyPath->setText(path);
    }
}

void SessionDialog::onHostChanged(const QString& text) {
    if (m_type->currentData().toString() != "ssh") {
        return;
    }
    if (m_name->text().trimmed().isEmpty()) {
        m_name->setText(text.trimmed());
    }
}

void SessionDialog::onProxyToggled(bool enabled) {
    const bool allow = enabled && m_type->currentData().toString() == "ssh";
    m_proxyHost->setEnabled(allow);
    m_proxyPort->setEnabled(allow);
    m_proxyUsername->setEnabled(allow);
    m_proxyPassword->setEnabled(allow);
}

void SessionDialog::onTunnelModeChanged() {
    const bool isSsh = m_type->currentData().toString() == "ssh";
    const QString mode = m_tunnelMode->currentData().toString();
    const bool enabled = isSsh && mode != "none";
    m_tunnelLocalPort->setEnabled(enabled);
    m_tunnelRemoteHost->setEnabled(enabled && mode != "dynamic");
    m_tunnelRemotePort->setEnabled(enabled && mode != "dynamic");
}

} // namespace svy::ui
