#include "ui/MainWindow.h"

#include <QAction>
#include <QDockWidget>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "core/SessionTypes.h"
#include "terminal/TerminalWidget.h"

namespace svy::ui {

MainWindow::MainWindow(svy::core::SessionManager* sessionManager, QWidget* parent)
    : QMainWindow(parent),
      m_sessionManager(sessionManager),
      m_sessionList(new QListWidget(this)),
      m_tabs(new QTabWidget(this)) {
    setWindowTitle("SVY-Term");
    resize(1280, 820);

    m_tabs->setTabsClosable(true);
    setCentralWidget(m_tabs);

    auto* sessionsDock = new QDockWidget("Sessions", this);
    sessionsDock->setWidget(m_sessionList);
    sessionsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, sessionsDock);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        QWidget* tab = m_tabs->widget(index);
        m_tabs->removeTab(index);
        delete tab;
    });

    connect(m_sessionManager, &svy::core::SessionManager::sessionsChanged,
            this, &MainWindow::onSessionsChanged);
    connect(m_sessionList, &QListWidget::itemDoubleClicked, this, [this]() {
        onOpenSelectedSession();
    });

    buildMenu();
    refreshSessionList();
}

svy::terminal::TerminalWidget* MainWindow::createLocalTab(const QString& title) {
    auto* terminal = new svy::terminal::TerminalWidget(this);
    const int tabIndex = m_tabs->addTab(terminal, title);
    m_tabs->setCurrentIndex(tabIndex);
    return terminal;
}

void MainWindow::createSshScaffoldTab(const svy::core::SessionProfile& profile) {
    auto* terminal = new svy::terminal::TerminalWidget(this);
    const int tabIndex = m_tabs->addTab(terminal, profile.name.isEmpty() ? "SSH" : profile.name);
    m_tabs->setCurrentIndex(tabIndex);

    const QString sshCmd = QString("ssh %1@%2 -p %3")
                               .arg(profile.username.isEmpty() ? "user" : profile.username,
                                    profile.host.isEmpty() ? "host" : profile.host)
                               .arg(profile.port);
    terminal->runCommand(sshCmd);
}

void MainWindow::onAddLocalSession() {
    svy::core::SessionProfile profile = m_sessionManager->createDefaultLocalSession();
    if (m_sessionManager->upsert(profile)) {
        createLocalTab(profile.name);
    }
}

void MainWindow::onAddSshSession() {
    svy::core::SessionProfile profile = m_sessionManager->createDefaultSshSession();
    profile.name = "SSH new session";
    profile.host = "example.com";
    profile.username = "user";

    if (m_sessionManager->upsert(profile)) {
        createSshScaffoldTab(profile);
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
        createSshScaffoldTab(profile);
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

    connect(newLocal, &QAction::triggered, this, &MainWindow::onAddLocalSession);
    connect(newSsh, &QAction::triggered, this, &MainWindow::onAddSshSession);
    connect(openSelected, &QAction::triggered, this, &MainWindow::onOpenSelectedSession);
    connect(quit, &QAction::triggered, this, &MainWindow::close);
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

} // namespace svy::ui
