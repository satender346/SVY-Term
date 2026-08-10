#include "ui/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDockWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPalette>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

#include <QUuid>
#include <QRegularExpression>
#include <QSysInfo>

#include "core/SessionTypes.h"
#include "protocols/CredentialCache.h"
#include "protocols/SftpClient.h"
#include "terminal/SshTerminalWidget.h"
#include "terminal/TerminalWidget.h"
#include "tunnels/TunnelManager.h"
#include "ui/SessionDialog.h"

namespace svy::ui {

MainWindow::MainWindow(svy::core::SessionManager* sessionManager, QWidget* parent)
    : QMainWindow(parent),
      m_sessionManager(sessionManager),
      m_sessionList(new QListWidget(this)),
            m_tabs(new QTabWidget(this)),
            m_sftpClient(new svy::protocols::SftpClient(this)),
            m_tunnelManager(new svy::tunnels::TunnelManager(this)),
        m_tunnelTable(new QTableWidget(this)),
            m_sftpList(new QListWidget(this)),
            m_sftpPath(new QLineEdit(this)) {
    setWindowTitle("SVY-Term");
    resize(1280, 820);

    m_tabs->setTabsClosable(true);
    setCentralWidget(m_tabs);

    auto* sessionsDock = new QDockWidget("Sessions", this);
    sessionsDock->setWidget(m_sessionList);
    sessionsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, sessionsDock);

    auto* tunnelsDock = new QDockWidget("SSH Tunnels", this);
    auto* tunnelsContainer = new QWidget(this);
    auto* tunnelsLayout = new QVBoxLayout(tunnelsContainer);
    auto* tunnelsButtons = new QHBoxLayout();
    auto* addTunnelButton = new QPushButton("New", tunnelsContainer);
    auto* startTunnelButton = new QPushButton("Start", tunnelsContainer);
    auto* stopTunnelButton = new QPushButton("Stop", tunnelsContainer);
    auto* editTunnelButton = new QPushButton("Edit", tunnelsContainer);
    auto* deleteTunnelButton = new QPushButton("Delete", tunnelsContainer);

    m_tunnelTable->setColumnCount(5);
    m_tunnelTable->setHorizontalHeaderLabels({"Name", "Mode", "Forward", "Gateway", "Status"});
    m_tunnelTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tunnelTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tunnelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tunnelTable->verticalHeader()->setVisible(false);
    m_tunnelTable->horizontalHeader()->setStretchLastSection(true);
    m_tunnelTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tunnelTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tunnelTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tunnelTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tunnelTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    tunnelsButtons->addWidget(addTunnelButton);
    tunnelsButtons->addWidget(startTunnelButton);
    tunnelsButtons->addWidget(stopTunnelButton);
    tunnelsButtons->addWidget(editTunnelButton);
    tunnelsButtons->addWidget(deleteTunnelButton);
    tunnelsLayout->addLayout(tunnelsButtons);
    tunnelsLayout->addWidget(m_tunnelTable);
    tunnelsContainer->setLayout(tunnelsLayout);
    tunnelsDock->setWidget(tunnelsContainer);
    tunnelsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, tunnelsDock);

    auto* sftpDock = new QDockWidget("SFTP Browser", this);
    auto* sftpContainer = new QWidget(this);
    auto* sftpLayout = new QVBoxLayout(sftpContainer);
    auto* sftpTop = new QHBoxLayout();
    auto* refreshButton = new QPushButton("Refresh", sftpContainer);
    auto* uploadButton = new QPushButton("Upload", sftpContainer);
    auto* downloadButton = new QPushButton("Download", sftpContainer);
    m_sftpPath->setPlaceholderText("Remote path (example: . or /home/user)");
    m_sftpPath->setText(".");
    sftpTop->addWidget(m_sftpPath);
    sftpTop->addWidget(uploadButton);
    sftpTop->addWidget(downloadButton);
    sftpTop->addWidget(refreshButton);
    sftpLayout->addLayout(sftpTop);
    sftpLayout->addWidget(m_sftpList);
    sftpContainer->setLayout(sftpLayout);
    sftpDock->setWidget(sftpContainer);
    sftpDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, sftpDock);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        QWidget* tab = m_tabs->widget(index);
        m_tabs->removeTab(index);
        delete tab;
    });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        auto* sshTab = qobject_cast<svy::terminal::SshTerminalWidget*>(m_tabs->widget(index));
        if (sshTab != nullptr) {
            bindSftpToSession(sshTab->profile());
        }
    });

    connect(m_sessionManager, &svy::core::SessionManager::sessionsChanged,
            this, &MainWindow::onSessionsChanged);
    connect(m_sessionList, &QListWidget::itemDoubleClicked, this, [this]() {
        onOpenSelectedSession();
    });
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshSftpDirectory);
    connect(uploadButton, &QPushButton::clicked, this, &MainWindow::onUploadToSftp);
    connect(downloadButton, &QPushButton::clicked, this, &MainWindow::onDownloadFromSftp);
    connect(m_sftpPath, &QLineEdit::returnPressed, this, &MainWindow::onRefreshSftpDirectory);
    connect(m_sftpList, &QListWidget::itemDoubleClicked, this, [this]() { onSftpItemActivated(); });
    connect(m_sftpClient, &svy::protocols::SftpClient::info, this, [this](const QString& message) {
        statusBar()->showMessage(message, 4000);
    });
    connect(m_sftpClient, &svy::protocols::SftpClient::errorOccurred, this, [this](const QString& message) {
        QMessageBox::warning(this, "SFTP", message);
    });
    connect(m_tunnelManager, &svy::tunnels::TunnelManager::tunnelStarted,
            this, [this](const svy::tunnels::TunnelProfile& t) {
                statusBar()->showMessage(QString("Tunnel started: %1").arg(t.name), 5000);
            refreshTunnelTable();
            });
        connect(m_tunnelManager, &svy::tunnels::TunnelManager::tunnelStopped,
            this, [this](const QString&) { refreshTunnelTable(); });
    connect(m_tunnelManager, &svy::tunnels::TunnelManager::errorOccurred,
            this, [this](const QString& m) { QMessageBox::warning(this, "Tunnel", m); });
        connect(addTunnelButton, &QPushButton::clicked, this, &MainWindow::onCreateTunnel);
        connect(startTunnelButton, &QPushButton::clicked, this, &MainWindow::onStartSelectedTunnel);
        connect(stopTunnelButton, &QPushButton::clicked, this, &MainWindow::onStopSelectedTunnel);
        connect(editTunnelButton, &QPushButton::clicked, this, &MainWindow::onEditSelectedTunnel);
        connect(deleteTunnelButton, &QPushButton::clicked, this, &MainWindow::onDeleteSelectedTunnel);
        connect(m_tunnelTable, &QTableWidget::doubleClicked, this, [this]() { onEditSelectedTunnel(); });

    buildMenu();
    refreshSessionList();
        refreshTunnelTable();
}

svy::terminal::TerminalWidget* MainWindow::createLocalTab(const QString& title) {
    auto* terminal = new svy::terminal::TerminalWidget(this);
    connect(terminal, &svy::terminal::TerminalWidget::sshCommandRequested,
            this, &MainWindow::handleLocalSshCommand);
    const int tabIndex = m_tabs->addTab(terminal, title);
    m_tabs->setCurrentIndex(tabIndex);
    return terminal;
}

void MainWindow::createSshTab(const svy::core::SessionProfile& profile) {
    auto* sshTerminal = new svy::terminal::SshTerminalWidget(profile, this);
    connect(sshTerminal, &svy::terminal::SshTerminalWidget::sshCommandRequested,
            this, [this, sshTerminal](const QString& command) {
                handleSshCommand(command, sshTerminal->profile().username);
            });
    const QString label = profile.host.trimmed().isEmpty()
                              ? (profile.name.isEmpty() ? "SSH" : profile.name)
                              : QString("%1 (%2)").arg(profile.name.isEmpty() ? profile.host : profile.name,
                                                        profile.host);
    const int tabIndex = m_tabs->addTab(sshTerminal, label);
    m_tabs->setCurrentIndex(tabIndex);
    bindSftpToSession(profile);
}

void MainWindow::onAddLocalSession() {
    svy::core::SessionProfile profile = m_sessionManager->createDefaultLocalSession();
    const QString id = profile.id;
    SessionDialog dialog(this);
    dialog.setWindowTitle("New Local Session");
    dialog.setProfile(profile);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    profile = dialog.profile();
    if (profile.name.isEmpty()) {
        profile.name = "Local";
    }
    profile.id = id;
    profile.type = svy::core::SessionType::Local;

    if (m_sessionManager->upsert(profile)) {
        createLocalTab(profile.name);
    }
}

void MainWindow::onAddSshSession() {
    svy::core::SessionProfile profile = m_sessionManager->createDefaultSshSession();
    const QString id = profile.id;
    SessionDialog dialog(this);
    dialog.setWindowTitle("New SSH Session");
    dialog.setProfile(profile);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    profile = dialog.profile();
    if (profile.name.isEmpty()) {
        profile.name = profile.host.trimmed().isEmpty() ? "SSH" : profile.host.trimmed();
    }
    profile.id = id;
    profile.type = svy::core::SessionType::SSH;

    if (m_sessionManager->upsert(profile)) {
        createSshTab(profile);
    }
}

void MainWindow::onEditSelectedSession() {
    const auto profile = currentSelectedSession();
    if (profile.id.isEmpty()) {
        QMessageBox::information(this, "Sessions", "Select a session first.");
        return;
    }

    SessionDialog dialog(this);
    dialog.setWindowTitle("Edit Session");
    dialog.setProfile(profile);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    auto updated = dialog.profile();
    updated.id = profile.id;
    if (updated.name.isEmpty()) {
        updated.name = profile.name;
    }
    if (updated.type == svy::core::SessionType::SSH && updated.port <= 0) {
        updated.port = 22;
    }

    m_sessionManager->upsert(updated);
}

void MainWindow::onDeleteSelectedSession() {
    const QString sessionId = currentSelectedSessionId();
    if (sessionId.isEmpty()) {
        QMessageBox::information(this, "Sessions", "Select a session first.");
        return;
    }

    const auto profile = m_sessionManager->findById(sessionId);
    const QString displayName = profile.name.isEmpty() ? sessionId : profile.name;

    const auto answer = QMessageBox::question(this, "Delete session",
                                              QString("Delete session '%1'?").arg(displayName));
    if (answer == QMessageBox::Yes) {
        if (!m_sessionManager->removeById(sessionId)) {
            QMessageBox::warning(this, "Delete session", "Unable to delete selected session.");
        }
    }
}

void MainWindow::onOpenSftpForSelectedSession() {
    const auto profile = currentSelectedSession();
    if (profile.id.isEmpty() || profile.type != svy::core::SessionType::SSH) {
        QMessageBox::information(this, "SFTP", "Select an SSH session first.");
        return;
    }

    bindSftpToSession(profile);
}

void MainWindow::onRefreshSftpDirectory() {
    if (!m_sftpClient->isConnected()) {
        m_sftpList->clear();
        m_sftpList->addItem("[not connected]");
        return;
    }

    const QString path = m_sftpPath->text().trimmed().isEmpty() ? "." : m_sftpPath->text().trimmed();
    const auto entries = m_sftpClient->listDirectory(path);

    m_sftpList->clear();
    for (const auto& e : entries) {
        m_sftpList->addItem(e);
    }

    if (entries.isEmpty()) {
        m_sftpList->addItem("[empty or unavailable]");
    }
}

void MainWindow::onUploadToSftp() {
    if (!m_sftpClient->isConnected()) {
        QMessageBox::information(this, "SFTP", "Connect an SSH/SFTP session first.");
        return;
    }

    const QString localFile = QFileDialog::getOpenFileName(this, "Select file to upload");
    if (localFile.isEmpty()) {
        return;
    }

    const QFileInfo localInfo(localFile);
    const QString basePath = m_sftpPath->text().trimmed().isEmpty() ? "." : m_sftpPath->text().trimmed();
    const QString suggestedRemote = buildRemotePath(basePath, localInfo.fileName());

    bool ok = false;
    const QString remoteFile = QInputDialog::getText(
        this,
        "Upload to SFTP",
        "Remote destination path",
        QLineEdit::Normal,
        suggestedRemote,
        &ok);
    if (!ok || remoteFile.trimmed().isEmpty()) {
        return;
    }

    if (m_sftpClient->upload(localFile, remoteFile.trimmed())) {
        onRefreshSftpDirectory();
    }
}

void MainWindow::onDownloadFromSftp() {
    if (!m_sftpClient->isConnected()) {
        QMessageBox::information(this, "SFTP", "Connect an SSH/SFTP session first.");
        return;
    }

    const auto selected = m_sftpList->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "SFTP", "Select a remote file entry first.");
        return;
    }

    QString entryName = selected.first()->text().trimmed();
    if (entryName.endsWith('/')) {
        QMessageBox::information(this, "SFTP", "Selected entry is a directory. Open it first, then select a file.");
        return;
    }

    const QString basePath = m_sftpPath->text().trimmed().isEmpty() ? "." : m_sftpPath->text().trimmed();
    const QString remoteFile = buildRemotePath(basePath, entryName);

    const QString localFile = QFileDialog::getSaveFileName(this, "Save downloaded file as", entryName);
    if (localFile.isEmpty()) {
        return;
    }

    m_sftpClient->download(remoteFile, localFile);
}

void MainWindow::onSftpItemActivated() {
    const auto selected = m_sftpList->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    QString entry = selected.first()->text().trimmed();
    if (!entry.endsWith('/')) {
        return;
    }

    entry.chop(1);
    const QString basePath = m_sftpPath->text().trimmed().isEmpty() ? "." : m_sftpPath->text().trimmed();
    const QString nextPath = buildRemotePath(basePath, entry);
    m_sftpPath->setText(nextPath);
    onRefreshSftpDirectory();
}

void MainWindow::onSessionsChanged() {
    refreshSessionList();
}

void MainWindow::onOpenSelectedSession() {
    const auto selected = m_sessionList->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    const QString sessionId = selected.first()->data(Qt::UserRole).toString();
    const svy::core::SessionProfile profile = m_sessionManager->findById(sessionId);

    if (profile.id.isEmpty()) {
        QMessageBox::warning(this, "Session", "Session was not found.");
        return;
    }

    if (profile.type == svy::core::SessionType::Local) {
        createLocalTab(profile.name);
    } else if (profile.type == svy::core::SessionType::SSH) {
        createSshTab(profile);
    } else {
        QMessageBox::information(this, "Session", "This session type is scaffolded and will be implemented next.");
    }
}

void MainWindow::buildMenu() {
    QMenu* fileMenu = menuBar()->addMenu("File");
    QAction* newLocal = fileMenu->addAction("New Local Tab");
    QAction* newSsh = fileMenu->addAction("New SSH Session");
    fileMenu->addSeparator();
    QAction* quit = fileMenu->addAction("Quit");

    QMenu* sessionsMenu = menuBar()->addMenu("Sessions");
    QAction* openSelected = sessionsMenu->addAction("Open Selected Session");
    QAction* editSelected = sessionsMenu->addAction("Edit Selected Session");
    QAction* deleteSelected = sessionsMenu->addAction("Delete Selected Session");
    sessionsMenu->addSeparator();
    QAction* openSftp = sessionsMenu->addAction("Open SFTP Browser (Selected SSH)");

    QMenu* toolsMenu = menuBar()->addMenu("Tools");
    QAction* broadcast = toolsMenu->addAction("Multi-exec: Broadcast command");
    toolsMenu->addSeparator();
    QAction* splitTwo = toolsMenu->addAction("Split terminals (2 panes)");
    QAction* splitFour = toolsMenu->addAction("Split terminals (4 panes)");
    toolsMenu->addSeparator();
    QAction* createTunnel = toolsMenu->addAction("New SSH tunnel (port forwarding)");
    QAction* editTunnel = toolsMenu->addAction("Edit selected tunnel");
    QAction* deleteTunnel = toolsMenu->addAction("Delete selected tunnel");
    QAction* startTunnel = toolsMenu->addAction("Start selected tunnel");
    QAction* stopTunnel = toolsMenu->addAction("Stop selected tunnel");
    QAction* stopTunnels = toolsMenu->addAction("Stop all tunnels");

    QMenu* viewMenu = menuBar()->addMenu("View");
    QAction* zoomIn = viewMenu->addAction("Zoom In");
    QAction* zoomOut = viewMenu->addAction("Zoom Out");
    QAction* zoomReset = viewMenu->addAction("Reset Zoom");
    viewMenu->addSeparator();
    QMenu* themeMenu = viewMenu->addMenu("Theme");
    QAction* themeLight = themeMenu->addAction("Light");
    QAction* themeDark = themeMenu->addAction("Dark");
    QAction* themeSystem = themeMenu->addAction("System");

    QMenu* helpMenu = menuBar()->addMenu("Help");
    QAction* aboutAction = helpMenu->addAction("About SVY-Term");

    connect(newLocal, &QAction::triggered, this, &MainWindow::onAddLocalSession);
    connect(newSsh, &QAction::triggered, this, &MainWindow::onAddSshSession);
    connect(openSelected, &QAction::triggered, this, &MainWindow::onOpenSelectedSession);
    connect(editSelected, &QAction::triggered, this, &MainWindow::onEditSelectedSession);
    connect(deleteSelected, &QAction::triggered, this, &MainWindow::onDeleteSelectedSession);
    connect(openSftp, &QAction::triggered, this, &MainWindow::onOpenSftpForSelectedSession);
    connect(broadcast, &QAction::triggered, this, &MainWindow::onBroadcastCommand);
    connect(splitTwo, &QAction::triggered, this, &MainWindow::onOpenSplitTwo);
    connect(splitFour, &QAction::triggered, this, &MainWindow::onOpenSplitFour);
    connect(createTunnel, &QAction::triggered, this, &MainWindow::onCreateTunnel);
    connect(editTunnel, &QAction::triggered, this, &MainWindow::onEditSelectedTunnel);
    connect(deleteTunnel, &QAction::triggered, this, &MainWindow::onDeleteSelectedTunnel);
    connect(startTunnel, &QAction::triggered, this, &MainWindow::onStartSelectedTunnel);
    connect(stopTunnel, &QAction::triggered, this, &MainWindow::onStopSelectedTunnel);
    connect(stopTunnels, &QAction::triggered, this, &MainWindow::onStopAllTunnels);
    connect(zoomIn, &QAction::triggered, this, &MainWindow::onZoomIn);
    connect(zoomOut, &QAction::triggered, this, &MainWindow::onZoomOut);
    connect(zoomReset, &QAction::triggered, this, &MainWindow::onZoomReset);
    connect(themeLight, &QAction::triggered, this, &MainWindow::onThemeLight);
    connect(themeDark, &QAction::triggered, this, &MainWindow::onThemeDark);
    connect(themeSystem, &QAction::triggered, this, &MainWindow::onThemeSystem);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onHelpAbout);
    connect(quit, &QAction::triggered, this, &MainWindow::close);
}

void MainWindow::onBroadcastCommand() {
    bool ok = false;
    const QString command = QInputDialog::getText(
        this,
        "Multi-exec",
        "Command to execute in all open local/SSH tabs:",
        QLineEdit::Normal,
        QString(),
        &ok);
    if (!ok || command.trimmed().isEmpty()) {
        return;
    }

    int count = 0;
    for (int i = 0; i < m_tabs->count(); ++i) {
        QWidget* tab = m_tabs->widget(i);
        if (auto* local = qobject_cast<svy::terminal::TerminalWidget*>(tab)) {
            local->runCommand(command);
            ++count;
        } else if (auto* ssh = qobject_cast<svy::terminal::SshTerminalWidget*>(tab)) {
            ssh->runCommand(command);
            ++count;
        }
    }

    statusBar()->showMessage(QString("Broadcast to %1 tab(s)").arg(count), 4000);
}

void MainWindow::onCreateTunnel() {
    const auto selected = currentSelectedSession();
    svy::tunnels::TunnelProfile draft;
    draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    draft.mode = "local";
    draft.localPort = 8080;
    draft.remoteHost = "127.0.0.1";
    draft.remotePort = 22;
    if (selected.type == svy::core::SessionType::SSH) {
        draft.gatewayHost = selected.host.trimmed();
        draft.gatewayUser = selected.username.trimmed();
        draft.privateKeyPath = selected.privateKeyPath;
        draft.gatewayPassword = selected.password;
        if (draft.gatewayPassword.isEmpty()) {
            draft.gatewayPassword = svy::protocols::CredentialCache::getPassword(
                selected.username, selected.host, selected.port);
        }
    }

    if (!promptTunnelProfile(&draft, selected, false)) {
        return;
    }

    m_tunnelProfiles.push_back(draft);
    refreshTunnelTable();
    m_tunnelManager->startTunnel(draft);
}

void MainWindow::onEditSelectedTunnel() {
    const QString tunnelId = selectedTunnelId();
    if (tunnelId.isEmpty()) {
        QMessageBox::information(this, "Tunnel", "Select a tunnel first.");
        return;
    }

    for (int i = 0; i < m_tunnelProfiles.size(); ++i) {
        if (m_tunnelProfiles[i].id != tunnelId) {
            continue;
        }

        svy::tunnels::TunnelProfile edited = m_tunnelProfiles[i];
        const auto session = currentSelectedSession();
        if (!promptTunnelProfile(&edited, session, true)) {
            return;
        }
        edited.id = tunnelId;

        const auto activeTunnels = m_tunnelManager->activeTunnels();
        const bool wasActive = std::any_of(activeTunnels.cbegin(),
                           activeTunnels.cend(),
                           [&tunnelId](const svy::tunnels::TunnelProfile& t) { return t.id == tunnelId; });
        if (wasActive) {
            m_tunnelManager->stopTunnel(tunnelId);
        }

        m_tunnelProfiles[i] = edited;
        refreshTunnelTable();
        if (wasActive) {
            m_tunnelManager->startTunnel(edited);
        }
        return;
    }

    QMessageBox::warning(this, "Tunnel", "Selected tunnel was not found.");
}

void MainWindow::onDeleteSelectedTunnel() {
    const QString tunnelId = selectedTunnelId();
    if (tunnelId.isEmpty()) {
        QMessageBox::information(this, "Tunnel", "Select a tunnel first.");
        return;
    }

    for (int i = 0; i < m_tunnelProfiles.size(); ++i) {
        if (m_tunnelProfiles[i].id != tunnelId) {
            continue;
        }
        const QString displayName = m_tunnelProfiles[i].name.isEmpty() ? tunnelId : m_tunnelProfiles[i].name;
        const auto answer = QMessageBox::question(this, "Delete tunnel",
                                                  QString("Delete tunnel '%1'?").arg(displayName));
        if (answer != QMessageBox::Yes) {
            return;
        }

        m_tunnelManager->stopTunnel(tunnelId);
        m_tunnelProfiles.removeAt(i);
        refreshTunnelTable();
        return;
    }

    QMessageBox::warning(this, "Tunnel", "Selected tunnel was not found.");
}

void MainWindow::onStartSelectedTunnel() {
    const QString tunnelId = selectedTunnelId();
    if (tunnelId.isEmpty()) {
        QMessageBox::information(this, "Tunnel", "Select a tunnel first.");
        return;
    }

    for (const auto& tunnel : m_tunnelProfiles) {
        if (tunnel.id == tunnelId) {
            m_tunnelManager->startTunnel(tunnel);
            return;
        }
    }

    QMessageBox::warning(this, "Tunnel", "Selected tunnel was not found.");
}

void MainWindow::onStopSelectedTunnel() {
    const QString tunnelId = selectedTunnelId();
    if (tunnelId.isEmpty()) {
        QMessageBox::information(this, "Tunnel", "Select a tunnel first.");
        return;
    }

    if (!m_tunnelManager->stopTunnel(tunnelId)) {
        QMessageBox::information(this, "Tunnel", "Tunnel is not active.");
    }
    refreshTunnelTable();
}

void MainWindow::onStopAllTunnels() {
    const auto active = m_tunnelManager->activeTunnels();
    for (const auto& t : active) {
        m_tunnelManager->stopTunnel(t.id);
    }
    refreshTunnelTable();
    statusBar()->showMessage("Stopped all tunnels", 4000);
}

void MainWindow::onOpenSplitTwo() {
    createSplitTab(2);
}

void MainWindow::onOpenSplitFour() {
    createSplitTab(4);
}

void MainWindow::onZoomIn() {
    applyFontDeltaToCurrentTab(+1);
}

void MainWindow::onZoomOut() {
    applyFontDeltaToCurrentTab(-1);
}

void MainWindow::onZoomReset() {
    resetFontOnCurrentTab();
}

svy::core::SessionProfile MainWindow::currentSelectedSession() const {
    const QString sessionId = currentSelectedSessionId();
    if (sessionId.isEmpty()) {
        return {};
    }
    return m_sessionManager->findById(sessionId);
}

QString MainWindow::currentSelectedSessionId() const {
    const auto selected = m_sessionList->selectedItems();
    if (!selected.isEmpty()) {
        return selected.first()->data(Qt::UserRole).toString();
    }
    if (m_sessionList->currentItem() != nullptr) {
        return m_sessionList->currentItem()->data(Qt::UserRole).toString();
    }
    return {};
}

QString MainWindow::selectedTunnelId() const {
    if (m_tunnelTable == nullptr) {
        return {};
    }
    const int row = m_tunnelTable->currentRow();
    if (row < 0) {
        return {};
    }
    auto* item = m_tunnelTable->item(row, 0);
    if (item == nullptr) {
        return {};
    }
    return item->data(Qt::UserRole).toString();
}

void MainWindow::refreshTunnelTable() {
    if (m_tunnelTable == nullptr) {
        return;
    }

    QSet<QString> activeIds;
    for (const auto& active : m_tunnelManager->activeTunnels()) {
        activeIds.insert(active.id);
    }

    const QString currentId = selectedTunnelId();
    m_tunnelTable->setRowCount(m_tunnelProfiles.size());
    for (int row = 0; row < m_tunnelProfiles.size(); ++row) {
        const auto& tunnel = m_tunnelProfiles[row];
        const QString mode = tunnel.mode.trimmed().toLower();
        const QString displayName = tunnel.name.isEmpty() ? tunnel.id : tunnel.name;
        const QString forward = (mode == "dynamic")
                                    ? QString("%1:%2 -> SOCKS").arg(tunnel.localHost).arg(tunnel.localPort)
                                    : QString("%1:%2 -> %3:%4")
                                          .arg(tunnel.localHost)
                                          .arg(tunnel.localPort)
                                          .arg(tunnel.remoteHost)
                                          .arg(tunnel.remotePort);
        const QString gateway = tunnel.gatewayUser.isEmpty()
                                    ? QString("%1:%2").arg(tunnel.gatewayHost).arg(tunnel.gatewayPort)
                                    : QString("%1@%2:%3").arg(tunnel.gatewayUser, tunnel.gatewayHost).arg(tunnel.gatewayPort);
        const QString status = activeIds.contains(tunnel.id) ? "running" : "stopped";

        auto* nameItem = new QTableWidgetItem(displayName);
        nameItem->setData(Qt::UserRole, tunnel.id);
        m_tunnelTable->setItem(row, 0, nameItem);
        m_tunnelTable->setItem(row, 1, new QTableWidgetItem(mode));
        m_tunnelTable->setItem(row, 2, new QTableWidgetItem(forward));
        m_tunnelTable->setItem(row, 3, new QTableWidgetItem(gateway));
        m_tunnelTable->setItem(row, 4, new QTableWidgetItem(status));

        if (!currentId.isEmpty() && tunnel.id == currentId) {
            m_tunnelTable->selectRow(row);
        }
    }
}

bool MainWindow::promptTunnelProfile(svy::tunnels::TunnelProfile* tunnel,
                                     const svy::core::SessionProfile& defaults,
                                     bool editing) {
    if (tunnel == nullptr) {
        return false;
    }

    svy::tunnels::TunnelProfile draft = *tunnel;
    if (draft.gatewayHost.isEmpty() && defaults.type == svy::core::SessionType::SSH) {
        draft.gatewayHost = defaults.host.trimmed();
    }
    if (draft.gatewayUser.isEmpty() && defaults.type == svy::core::SessionType::SSH) {
        draft.gatewayUser = defaults.username.trimmed();
    }
    if (draft.gatewayPassword.isEmpty() && defaults.type == svy::core::SessionType::SSH) {
        draft.gatewayPassword = defaults.password;
    }
    if (draft.gatewayPort <= 0) {
        draft.gatewayPort = 22;
    }
    if (draft.localHost.trimmed().isEmpty()) {
        draft.localHost = "127.0.0.1";
    }
    if (draft.localPort <= 0) {
        draft.localPort = 8080;
    }
    if (draft.remoteHost.trimmed().isEmpty()) {
        draft.remoteHost = "127.0.0.1";
    }
    if (draft.remotePort <= 0) {
        draft.remotePort = 22;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(editing ? "Edit Tunnel" : "New Tunnel");
    auto* rootLayout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();

    auto* nameEdit = new QLineEdit(draft.name, &dialog);
    auto* modeBox = new QComboBox(&dialog);
    modeBox->addItem("Local", "local");
    modeBox->addItem("Remote", "remote");
    modeBox->addItem("Dynamic (SOCKS)", "dynamic");
    const int modeIndex = std::max(0, modeBox->findData(draft.mode.trimmed().toLower()));
    modeBox->setCurrentIndex(modeIndex);

    auto* gatewayHostEdit = new QLineEdit(draft.gatewayHost, &dialog);
    auto* gatewayUserEdit = new QLineEdit(draft.gatewayUser, &dialog);
    auto* gatewayPortSpin = new QSpinBox(&dialog);
    gatewayPortSpin->setRange(1, 65535);
    gatewayPortSpin->setValue(draft.gatewayPort);
    auto* passwordEdit = new QLineEdit(draft.gatewayPassword, &dialog);
    passwordEdit->setEchoMode(QLineEdit::Password);

    auto* localHostEdit = new QLineEdit(draft.localHost, &dialog);
    auto* localPortSpin = new QSpinBox(&dialog);
    localPortSpin->setRange(1, 65535);
    localPortSpin->setValue(draft.localPort);
    auto* remoteHostEdit = new QLineEdit(draft.remoteHost, &dialog);
    auto* remotePortSpin = new QSpinBox(&dialog);
    remotePortSpin->setRange(1, 65535);
    remotePortSpin->setValue(draft.remotePort);

    auto* keyPathEdit = new QLineEdit(draft.privateKeyPath, &dialog);
    auto* keyPathRow = new QWidget(&dialog);
    auto* keyPathLayout = new QHBoxLayout(keyPathRow);
    keyPathLayout->setContentsMargins(0, 0, 0, 0);
    auto* keyBrowse = new QPushButton("Browse", keyPathRow);
    keyPathLayout->addWidget(keyPathEdit);
    keyPathLayout->addWidget(keyBrowse);

    form->addRow("Name", nameEdit);
    form->addRow("Mode", modeBox);
    form->addRow("Gateway host", gatewayHostEdit);
    form->addRow("Gateway user", gatewayUserEdit);
    form->addRow("Gateway port", gatewayPortSpin);
    form->addRow("Gateway password", passwordEdit);
    form->addRow("Local bind host", localHostEdit);
    form->addRow("Local bind port", localPortSpin);
    form->addRow("Remote host", remoteHostEdit);
    form->addRow("Remote port", remotePortSpin);
    form->addRow("Private key", keyPathRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    rootLayout->addLayout(form);
    rootLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(keyBrowse, &QPushButton::clicked, &dialog, [&dialog, keyPathEdit]() {
        const QString selected = QFileDialog::getOpenFileName(&dialog, "Select private key");
        if (!selected.isEmpty()) {
            keyPathEdit->setText(selected);
        }
    });

    auto applyModeState = [modeBox, remoteHostEdit, remotePortSpin]() {
        const bool requiresRemote = modeBox->currentData().toString() != "dynamic";
        remoteHostEdit->setEnabled(requiresRemote);
        remotePortSpin->setEnabled(requiresRemote);
    };
    connect(modeBox, &QComboBox::currentIndexChanged, &dialog, [applyModeState](int) { applyModeState(); });
    applyModeState();

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    draft.name = nameEdit->text().trimmed();
    draft.mode = modeBox->currentData().toString();
    draft.gatewayHost = gatewayHostEdit->text().trimmed();
    draft.gatewayUser = gatewayUserEdit->text().trimmed();
    draft.gatewayPort = gatewayPortSpin->value();
    draft.gatewayPassword = passwordEdit->text();
    draft.localHost = localHostEdit->text().trimmed().isEmpty() ? QString("127.0.0.1") : localHostEdit->text().trimmed();
    draft.localPort = localPortSpin->value();
    draft.remoteHost = remoteHostEdit->text().trimmed();
    draft.remotePort = remotePortSpin->value();
    draft.privateKeyPath = keyPathEdit->text().trimmed();

    if (draft.gatewayHost.isEmpty()) {
        QMessageBox::warning(this, "Tunnel", "Gateway host is required.");
        return false;
    }

    if (draft.mode != "dynamic" && draft.remoteHost.isEmpty()) {
        QMessageBox::warning(this, "Tunnel", "Remote host is required for local/remote forwarding.");
        return false;
    }

    if (draft.name.isEmpty()) {
        if (draft.mode == "dynamic") {
            draft.name = QString("SOCKS %1:%2 via %3").arg(draft.localHost).arg(draft.localPort).arg(draft.gatewayHost);
        } else {
            draft.name = QString("%1 %2:%3 -> %4:%5")
                             .arg(draft.mode.toUpper(), draft.localHost)
                             .arg(draft.localPort)
                             .arg(draft.remoteHost)
                             .arg(draft.remotePort);
        }
    }

    *tunnel = draft;
    return true;
}

void MainWindow::refreshSessionList() {
    m_sessionList->clear();

    for (const auto& session : m_sessionManager->sessions()) {
        auto* item = new QListWidgetItem(
            QString("%1 [%2]").arg(session.name, svy::core::sessionTypeToString(session.type)),
            m_sessionList);
        item->setData(Qt::UserRole, session.id);
    }
}

void MainWindow::createSplitTab(int paneCount) {
    if (paneCount != 2 && paneCount != 4) {
        return;
    }

    QStringList choices;
    choices << "Local terminal";
    for (const auto& session : m_sessionManager->sessions()) {
        if (session.type == svy::core::SessionType::SSH) {
            choices << QString("SSH: %1").arg(session.name.isEmpty() ? session.host : session.name);
        }
    }

    auto* splitWidget = new QWidget(this);
    auto* grid = new QGridLayout(splitWidget);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setSpacing(6);

    const int columns = paneCount == 2 ? 2 : 2;
    for (int i = 0; i < paneCount; ++i) {
        bool ok = false;
        const QString choice = QInputDialog::getItem(
            this,
            "Split Pane Session",
            QString("Pane %1 session").arg(i + 1),
            choices,
            0,
            false,
            &ok);
        if (!ok) {
            delete splitWidget;
            return;
        }

        QWidget* pane = createPaneWidgetForChoice(choice, splitWidget);
        if (pane == nullptr) {
            auto* localFallback = new svy::terminal::TerminalWidget(splitWidget);
            connect(localFallback, &svy::terminal::TerminalWidget::sshCommandRequested,
                    this, &MainWindow::handleLocalSshCommand);
            pane = localFallback;
        }

        const int row = paneCount == 2 ? 0 : (i / columns);
        const int col = i % columns;
        grid->addWidget(pane, row, col);
    }

    const int tabIndex = m_tabs->addTab(splitWidget, QString("Split %1").arg(paneCount));
    m_tabs->setCurrentIndex(tabIndex);
}

QWidget* MainWindow::createPaneWidgetForChoice(const QString& choice, QWidget* parent) {
    if (choice == "Local terminal") {
    auto* terminal = new svy::terminal::TerminalWidget(parent);
    connect(terminal, &svy::terminal::TerminalWidget::sshCommandRequested,
        this, &MainWindow::handleLocalSshCommand);
    return terminal;
    }

    if (!choice.startsWith("SSH: ")) {
        return nullptr;
    }

    const QString name = choice.mid(5);
    for (const auto& session : m_sessionManager->sessions()) {
        if (session.type != svy::core::SessionType::SSH) {
            continue;
        }

        const QString label = session.name.isEmpty() ? session.host : session.name;
        if (label == name) {
            return new svy::terminal::SshTerminalWidget(session, parent);
        }
    }
    return nullptr;
}

void MainWindow::onThemeLight() {
    applyTheme("light");
}

void MainWindow::onThemeDark() {
    applyTheme("dark");
}

void MainWindow::onThemeSystem() {
    applyTheme("system");
}

void MainWindow::onHelpAbout() {
    const QString version = QCoreApplication::applicationVersion().isEmpty()
                                ? QStringLiteral("0.1.0")
                                : QCoreApplication::applicationVersion();
#if SVYTERM_HAS_LIBSSH
    const QString sshBackend = "libssh (enabled)";
#else
    const QString sshBackend = "fallback (libssh disabled)";
#endif

    const QString info = QString(
        "SVY-Term\n"
        "Release version: %1\n"
        "Build: %2 %3\n"
        "Qt: %4\n"
        "Platform: %5\n"
        "SSH backend: %6")
                             .arg(version)
                             .arg(QString::fromLatin1(__DATE__))
                             .arg(QString::fromLatin1(__TIME__))
                             .arg(QString::fromLatin1(QT_VERSION_STR))
                             .arg(QSysInfo::prettyProductName())
                             .arg(sshBackend);

    QMessageBox::about(this, "About SVY-Term", info);
}

void MainWindow::applyTheme(const QString& mode) {
    static const QPalette systemPalette = qApp->palette();

    if (mode == "dark") {
        QPalette p;
        p.setColor(QPalette::Window, QColor(23, 31, 45));
        p.setColor(QPalette::WindowText, QColor(229, 231, 235));
        p.setColor(QPalette::Base, QColor(11, 18, 32));
        p.setColor(QPalette::AlternateBase, QColor(31, 41, 55));
        p.setColor(QPalette::ToolTipBase, QColor(31, 41, 55));
        p.setColor(QPalette::ToolTipText, QColor(229, 231, 235));
        p.setColor(QPalette::Text, QColor(229, 231, 235));
        p.setColor(QPalette::Button, QColor(55, 65, 81));
        p.setColor(QPalette::ButtonText, QColor(229, 231, 235));
        p.setColor(QPalette::BrightText, Qt::red);
        p.setColor(QPalette::Link, QColor(96, 165, 250));
        p.setColor(QPalette::Highlight, QColor(59, 130, 246));
        p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        qApp->setPalette(p);
        qApp->setStyleSheet(QString());
        return;
    }

    if (mode == "light") {
        qApp->setPalette(systemPalette);
        qApp->setStyleSheet(QString());
        return;
    }

    qApp->setPalette(systemPalette);
    qApp->setStyleSheet(QString());
}

void MainWindow::applyFontDeltaToCurrentTab(int delta) {
    QWidget* tab = m_tabs->currentWidget();
    if (tab == nullptr) {
        return;
    }

    if (auto* local = qobject_cast<svy::terminal::TerminalWidget*>(tab)) {
        local->adjustFontSize(delta);
        return;
    }

    if (auto* ssh = qobject_cast<svy::terminal::SshTerminalWidget*>(tab)) {
        ssh->adjustFontSize(delta);
        return;
    }

    const auto localChildren = tab->findChildren<svy::terminal::TerminalWidget*>();
    for (auto* term : localChildren) {
        term->adjustFontSize(delta);
    }
    const auto sshChildren = tab->findChildren<svy::terminal::SshTerminalWidget*>();
    for (auto* term : sshChildren) {
        term->adjustFontSize(delta);
    }
}

void MainWindow::resetFontOnCurrentTab() {
    QWidget* tab = m_tabs->currentWidget();
    if (tab == nullptr) {
        return;
    }

    if (auto* local = qobject_cast<svy::terminal::TerminalWidget*>(tab)) {
        local->resetFontSize();
        return;
    }

    if (auto* ssh = qobject_cast<svy::terminal::SshTerminalWidget*>(tab)) {
        ssh->resetFontSize();
        return;
    }

    const auto localChildren = tab->findChildren<svy::terminal::TerminalWidget*>();
    for (auto* term : localChildren) {
        term->resetFontSize();
    }
    const auto sshChildren = tab->findChildren<svy::terminal::SshTerminalWidget*>();
    for (auto* term : sshChildren) {
        term->resetFontSize();
    }
}

void MainWindow::handleLocalSshCommand(const QString& command) {
    handleSshCommand(command, QString());
}

void MainWindow::handleSshCommand(const QString& command, const QString& fallbackUsername) {
    // Expected forms: ssh user@host, ssh host, ssh -p 2222 user@host
    QString user;
    QString host;
    int port = 22;

    const QStringList parts = command.simplified().split(' ', Qt::SkipEmptyParts);
    for (int i = 1; i < parts.size(); ++i) {
        const QString token = parts.at(i);
        if (token == "-p" && i + 1 < parts.size()) {
            bool ok = false;
            const int parsedPort = parts.at(i + 1).toInt(&ok);
            if (ok && parsedPort > 0) {
                port = parsedPort;
            }
            ++i;
            continue;
        }
        if (token.startsWith('-')) {
            continue;
        }
        if (token.contains('@')) {
            const QStringList uh = token.split('@');
            if (uh.size() == 2) {
                user = uh.at(0);
                host = uh.at(1);
            }
        } else {
            host = token;
        }
        break;
    }

    if (host.trimmed().isEmpty()) {
        statusBar()->showMessage("Unable to parse SSH target. Use: ssh user@host", 5000);
        return;
    }

    if (user.trimmed().isEmpty()) {
        user = fallbackUsername.trimmed();
    }

    svy::core::SessionProfile quick = m_sessionManager->createDefaultSshSession();
    quick.name = host.trimmed();
    quick.host = host.trimmed();
    quick.port = port;
    quick.username = user.trimmed();
    quick.type = svy::core::SessionType::SSH;

    createSshTab(quick);
    statusBar()->showMessage("Opened internal SSH tab for interactive login.", 5000);
}

QString MainWindow::buildRemotePath(const QString& basePath, const QString& entryName) const {
    QString base = basePath.trimmed();
    QString entry = entryName.trimmed();

    if (base.isEmpty()) {
        base = ".";
    }
    if (entry.startsWith('/')) {
        return entry;
    }
    if (base.endsWith('/')) {
        return base + entry;
    }
    return base + "/" + entry;
}

void MainWindow::bindSftpToSession(const svy::core::SessionProfile& profile) {
    if (profile.type != svy::core::SessionType::SSH || profile.host.trimmed().isEmpty()) {
        return;
    }

    if (!m_activeSftpSessionId.isEmpty() && m_activeSftpSessionId == profile.id) {
        onRefreshSftpDirectory();
        return;
    }

    if (m_sftpClient->connectSession(profile)) {
        m_activeSftpSessionId = profile.id;
        if (m_sftpPath->text().trimmed().isEmpty() || m_sftpPath->text().trimmed() == ".") {
            m_sftpPath->setText(".");
        }
        onRefreshSftpDirectory();
    }
}

} // namespace svy::ui
