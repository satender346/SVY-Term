#pragma once

#include <QApplication>
#include <memory>

namespace svy::config {
class AppConfig;
}

namespace svy::core {
class SessionManager;
}

namespace svy::ui {
class MainWindow;
}

namespace svy::app {

class Application {
public:
    Application(int& argc, char** argv);
    ~Application();

    int run();

private:
    struct CliOptions {
        QString newTabType;
        QString execCommand;
        QString configPath;
        bool hideTerm = false;
    };

    CliOptions parseCli() const;

    QApplication m_qapp;
    std::unique_ptr<svy::config::AppConfig> m_config;
    std::unique_ptr<svy::core::SessionManager> m_sessionManager;
    std::unique_ptr<svy::ui::MainWindow> m_mainWindow;
};

} // namespace svy::app
