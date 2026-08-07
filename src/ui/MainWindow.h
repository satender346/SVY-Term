#pragma once

#include <QMainWindow>

#include "core/SessionManager.h"

class QListWidget;
class QTabWidget;

namespace svy::terminal {
class TerminalWidget;
}

namespace svy::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(svy::core::SessionManager* sessionManager, QWidget* parent = nullptr);

    svy::terminal::TerminalWidget* createLocalTab(const QString& title = "Local");
    void createSshScaffoldTab(const svy::core::SessionProfile& profile);

private slots:
    void onAddLocalSession();
    void onAddSshSession();
    void onSessionsChanged();
    void onOpenSelectedSession();

private:
    void buildMenu();
    void refreshSessionList();

    svy::core::SessionManager* m_sessionManager;
    QListWidget* m_sessionList;
    QTabWidget* m_tabs;
};

} // namespace svy::ui
