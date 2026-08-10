#pragma once

#include <QDialog>

#include "core/SessionTypes.h"

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace svy::ui {

class SessionDialog : public QDialog {
    Q_OBJECT

public:
    explicit SessionDialog(QWidget* parent = nullptr);

    void setProfile(const svy::core::SessionProfile& profile);
    svy::core::SessionProfile profile() const;

private slots:
    void onTypeChanged();

private:
    QLineEdit* m_name;
    QComboBox* m_type;
    QLineEdit* m_host;
    QSpinBox* m_port;
    QLineEdit* m_username;
    QLineEdit* m_keyPath;
    QLineEdit* m_startupCommand;
};

} // namespace svy::ui
