#include "ui/MainWindow.h"

#include <QAction>
#include <QDockWidget>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <QUuid>

#include "core/SessionTypes.h"
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

    auto* sftpDock = new QDockWidget("SFTP Browser", this);
    auto* sftpContainer = new QWidget(this);
    auto* sftpLayout = new QVBoxLayout(sftpContainer);
    auto* sftpTop = new QHBoxLayout();
    auto* refreshButton = new QPushButton("Refresh", sftpContainer);
    m_sftpPath->setPlaceholderText("Remote path (example: . or /home/user)");
    m_sftpPath->setText(".");
    sftpTop->addWidget(m_sftpPath);
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
    connect(m_sftpPath, &QLineEdit::returnPressed, this, &MainWindow::onRefreshSftpDirectory);
    connect(m_sftpClient, &svy::protocols::SftpClient::info, this, [this](const QString& message) {
        statusBar()->showMessage(message, 4000);
    });
    connect(m_sftpClient, &svy::protocols::SftpClient::errorOccurred, this, [this](const QString& message) {
        QMessageBox::warning(this, "SFTP", message);
    });
    connect(m_tunnelManager, &svy::tunnels::TunnelManager::tunnelStarted,
            this, [this](const svy::tunnels::TunnelProfile& t) {
                statusBar()->showMessage(QString("Tunnel started: %1").arg(t.name), 5000);
            });
    connect(m_tunnelManager, &svy::tunnels::TunnelManager::errorOccurred,
            this, [this](const QString& m) { QMessageBox::warning(this, "Tunnel", m); });

    buildMenu();
    refreshSessionList();
}

svy::terminal::TerminalWidget* MainWindow::createLocalTab(const QString& title) {
    auto* terminal = new svy::terminal::TerminalWidget(this);
    const int tabIndex = m_tabs->addTab(terminal, title);
    m_tabs->setCurrentIndex(tabIndex);
    return terminal;
}

void MainWindow::createSshTab(const svy::core::SessionProfile& profile) {
    auto* sshTerminal = new svy::terminal::SshTerminalWidget(profile, this);
    const int tabIndex = m_tabs->addTab(sshTerminal, profile.name.isEmpty() ? "SSH" : profile.name);
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
    const auto profile = currentSelectedSession();
    if (profile.id.isEmpty()) {
        QMessageBox::information(this, "Sessions", "Select a session first.");
        return;
    }

    const auto answer = QMessageBox::question(this, "Delete session",
                                              QString("Delete session '%1'?").arg(profile.name));
    if (answer == QMessageBox::Yes) {
        m_sessionManager->removeById(profile.id);
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
    QAction* createTunnel = toolsMenu->addAction("Start SSH tunnel (port forwarding)");
    QAction* stopTunnels = toolsMenu->addAction("Stop all tunnels");

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
    connect(stopTunnels, &QAction::triggered, this, &MainWindow::onStopAllTunnels);
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
    bool ok = false;

    const auto selected = currentSelectedSession();

    const QString mode = QInputDialog::getItem(
        this,
        "Tunnel",
        "Mode",
        {"local", "remote", "dynamic"},
        0,
        false,
        &ok);
    if (!ok || mode.isEmpty()) {
        return;
    }

    const QString gatewayHost = QInputDialog::getText(
        this,
        "Tunnel",
        "Gateway host",
        QLineEdit::Normal,
        selected.host,
        &ok);
    if (!ok || gatewayHost.trimmed().isEmpty()) {
        return;
    }

    const QString gatewayUser = QInputDialog::getText(
        this,
        "Tunnel",
        "Gateway username",
        QLineEdit::Normal,
        selected.username,
        &ok);
    if (!ok) {
        return;
    }

    const int localPort = QInputDialog::getInt(this, "Tunnel", "Local port", 8080, 1, 65535, 1, &ok);
    if (!ok) {
        return;
    }

    QString remoteHost;
    int remotePort = 0;
    if (mode != "dynamic") {
        remoteHost = QInputDialog::getText(this, "Tunnel", "Remote host", QLineEdit::Normal, "127.0.0.1", &ok);
        if (!ok || remoteHost.trimmed().isEmpty()) {
            return;
        }

        remotePort = QInputDialog::getInt(this, "Tunnel", "Remote port", 22, 1, 65535, 1, &ok);
        if (!ok) {
            return;
        }
    }

    svy::tunnels::TunnelProfile t;
    t.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    t.mode = mode;
    if (mode == "dynamic") {
        t.name = QString("SOCKS %1:%2 via %3").arg(t.localHost).arg(localPort).arg(gatewayHost.trimmed());
    } else {
        t.name = QString("%1 %2:%3 -> %4:%5")
                     .arg(mode.toUpper(), t.localHost)
                     .arg(localPort)
                     .arg(remoteHost)
                     .arg(remotePort);
    }
    t.gatewayHost = gatewayHost.trimmed();
    t.gatewayUser = gatewayUser.trimmed();
    t.gatewayPort = 22;
    t.localPort = localPort;
    t.remoteHost = remoteHost.trimmed();
    t.remotePort = remotePort;
    t.privateKeyPath = selected.privateKeyPath;

    m_tunnelManager->startTunnel(t);
}

void MainWindow::onStopAllTunnels() {
    const auto active = m_tunnelManager->activeTunnels();
    for (const auto& t : active) {
        m_tunnelManager->stopTunnel(t.id);
    }
    statusBar()->showMessage("Stopped all tunnels", 4000);
}

void MainWindow::onOpenSplitTwo() {
    createSplitTab(2);
}

void MainWindow::onOpenSplitFour() {
    createSplitTab(4);
}

svy::core::SessionProfile MainWindow::currentSelectedSession() const {
    const auto selected = m_sessionList->selectedItems();
    if (selected.isEmpty()) {
        return {};
    }

    const QString sessionId = selected.first()->data(Qt::UserRole).toString();
    return m_sessionManager->findById(sessionId);
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

    auto* splitWidget = new QWidget(this);
    auto* grid = new QGridLayout(splitWidget);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setSpacing(6);

    const int columns = paneCount == 2 ? 2 : 2;
    for (int i = 0; i < paneCount; ++i) {
        auto* term = new svy::terminal::TerminalWidget(splitWidget);
        const int row = paneCount == 2 ? 0 : (i / columns);
        const int col = i % columns;
        grid->addWidget(term, row, col);
    }

    const int tabIndex = m_tabs->addTab(splitWidget, QString("Split %1").arg(paneCount));
    m_tabs->setCurrentIndex(tabIndex);
}

void MainWindow::bindSftpToSession(const svy::core::SessionProfile& profile) {
    if (profile.type != svy::core::SessionType::SSH || profile.host.trimmed().isEmpty()) {
        return;
    }

    if (m_sftpClient->connectSession(profile)) {
        if (m_sftpPath->text().trimmed().isEmpty() || m_sftpPath->text().trimmed() == ".") {
            m_sftpPath->setText(".");
        }
        onRefreshSftpDirectory();
    }
}

} // namespace svy::ui
