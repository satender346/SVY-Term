#pragma once

#include <QDialog>

#include "core/SessionTypes.h"

class QComboBox;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QPushButton;

namespace svy::ui {

class SessionDialog : public QDialog {
    Q_OBJECT

public:
    explicit SessionDialog(QWidget* parent = nullptr);

    void setProfile(const svy::core::SessionProfile& profile);
    svy::core::SessionProfile profile() const;

private slots:
    void onTypeChanged();
    void onBrowseKeyPath();
    void onHostChanged(const QString& text);
    void onProxyToggled(bool enabled);
    void onTunnelModeChanged();

private:
    QLineEdit* m_name;
    QComboBox* m_type;
    QLineEdit* m_host;
    QSpinBox* m_port;
    QLineEdit* m_username;
    QLineEdit* m_password;
    QLineEdit* m_keyPath;
    QPushButton* m_browseKeyPath;

    QCheckBox* m_useProxy;
    QLineEdit* m_proxyHost;
    QSpinBox* m_proxyPort;
    QLineEdit* m_proxyUsername;
    QLineEdit* m_proxyPassword;

    QComboBox* m_tunnelMode;
    QSpinBox* m_tunnelLocalPort;
    QLineEdit* m_tunnelRemoteHost;
    QSpinBox* m_tunnelRemotePort;

    QLineEdit* m_startupCommand;
};

} // namespace svy::ui
