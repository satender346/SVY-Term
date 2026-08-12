#pragma once

#include <QMainWindow>

#include "core/SessionManager.h"
#include "tunnels/TunnelManager.h"

class QDockWidget;
class QListWidget;
class QLineEdit;
class QTableWidget;
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
    void onSftpGoUp();
    void onSftpGoHome();
    void onToggleBroadcast(bool enabled);
    void onTerminalCommandEntered(const QString& command);
    void onCreateTunnel();
    void onEditSelectedTunnel();
    void onDeleteSelectedTunnel();
    void onStartSelectedTunnel();
    void onStopSelectedTunnel();
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
    void refreshTunnelTable();
    QString selectedTunnelId() const;
    bool promptTunnelProfile(svy::tunnels::TunnelProfile* tunnel,
                             const svy::core::SessionProfile& defaults,
                             bool editing);

    svy::core::SessionManager* m_sessionManager;
    QListWidget* m_sessionList;
    QTabWidget* m_tabs;
    svy::protocols::SftpClient* m_sftpClient;
    svy::tunnels::TunnelManager* m_tunnelManager;
    QTableWidget* m_tunnelTable;
    QListWidget* m_sftpList;
    QLineEdit* m_sftpPath;
    QDockWidget* m_sessionsDock = nullptr;
    QDockWidget* m_tunnelsDock = nullptr;
    QDockWidget* m_sftpDock = nullptr;
    QString m_activeSftpSessionId;
    QVector<svy::tunnels::TunnelProfile> m_tunnelProfiles;
    bool m_broadcastMode = false;
};

} // namespace svy::ui
