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

namespace svy::tunnels {
class TunnelManager;
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
    void onUploadToSftp();
    void onDownloadFromSftp();
    void onSftpItemActivated();
    void onBroadcastCommand();
    void onCreateTunnel();
    void onStopAllTunnels();
    void onOpenSplitTwo();
    void onOpenSplitFour();
    void onZoomIn();
    void onZoomOut();
    void onZoomReset();
    void onThemeLight();
    void onThemeDark();
    void onThemeSystem();
    void onHelpAbout();
    void onSessionsChanged();
    void onOpenSelectedSession();

private:
    svy::core::SessionProfile currentSelectedSession() const;
    QString currentSelectedSessionId() const;
    void buildMenu();
    void refreshSessionList();
    void createSplitTab(int paneCount);
    void bindSftpToSession(const svy::core::SessionProfile& profile);
    QWidget* createPaneWidgetForChoice(const QString& choice, QWidget* parent);
    void applyTheme(const QString& mode);
    void applyFontDeltaToCurrentTab(int delta);
    void resetFontOnCurrentTab();
    void handleLocalSshCommand(const QString& command);
    void handleSshCommand(const QString& command, const QString& fallbackUsername = QString());
    QString buildRemotePath(const QString& basePath, const QString& entryName) const;

    svy::core::SessionManager* m_sessionManager;
    QListWidget* m_sessionList;
    QTabWidget* m_tabs;
    svy::protocols::SftpClient* m_sftpClient;
    svy::tunnels::TunnelManager* m_tunnelManager;
    QListWidget* m_sftpList;
    QLineEdit* m_sftpPath;
    QString m_activeSftpSessionId;
};

} // namespace svy::ui
