#pragma once

#include <QMainWindow>

#include "core/SessionManager.h"

class QListWidget;
class QLineEdit;
class QTabWidget;

namespace svy::terminal {
class TerminalWidget;
class SshTerminalWidget;
}

namespace svy::protocols {
class SftpClient;
}

namespace svy::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(svy::core::SessionManager* sessionManager, QWidget* parent = nullptr);

    svy::terminal::TerminalWidget* createLocalTab(const QString& title = "Local");
    void createSshTab(const svy::core::SessionProfile& profile);

private slots:
    void onAddLocalSession();
    void onAddSshSession();
    void onEditSelectedSession();
    void onDeleteSelectedSession();
    void onOpenSftpForSelectedSession();
    void onRefreshSftpDirectory();
    void onSessionsChanged();
    void onOpenSelectedSession();

private:
    svy::core::SessionProfile currentSelectedSession() const;
    void buildMenu();
    void refreshSessionList();

    svy::core::SessionManager* m_sessionManager;
    QListWidget* m_sessionList;
    QTabWidget* m_tabs;
    svy::protocols::SftpClient* m_sftpClient;
    QListWidget* m_sftpList;
    QLineEdit* m_sftpPath;
};

} // namespace svy::ui
