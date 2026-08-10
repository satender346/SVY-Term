#include "ui/SessionDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
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
      m_keyPath(new QLineEdit(this)),
      m_startupCommand(new QLineEdit(this)) {
    setWindowTitle("Session");
    resize(560, 260);

    m_type->addItem("Local", "local");
    m_type->addItem("SSH", "ssh");

    m_port->setRange(1, 65535);
    m_port->setValue(22);

    auto* form = new QFormLayout();
    form->addRow("Name", m_name);
    form->addRow("Type", m_type);
    form->addRow("Host", m_host);
    form->addRow("Port", m_port);
    form->addRow("Username", m_username);
    form->addRow("Private key path", m_keyPath);
    form->addRow("Startup command", m_startupCommand);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(m_type, &QComboBox::currentIndexChanged, this, [this](int) {
        onTypeChanged();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    onTypeChanged();
}

void SessionDialog::setProfile(const svy::core::SessionProfile& p) {
    m_name->setText(p.name);
    m_type->setCurrentIndex(p.type == svy::core::SessionType::SSH ? 1 : 0);
    m_host->setText(p.host);
    m_port->setValue(p.port > 0 ? p.port : 22);
    m_username->setText(p.username);
    m_keyPath->setText(p.privateKeyPath);
    m_startupCommand->setText(p.startupCommand);
    onTypeChanged();
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
    p.privateKeyPath = m_keyPath->text().trimmed();
    p.startupCommand = m_startupCommand->text().trimmed();
    return p;
}

void SessionDialog::onTypeChanged() {
    const bool isSsh = m_type->currentData().toString() == "ssh";
    m_host->setEnabled(isSsh);
    m_port->setEnabled(isSsh);
    m_username->setEnabled(isSsh);
    m_keyPath->setEnabled(isSsh);
}

} // namespace svy::ui
