#include "app/Application.h"

#include <QCommandLineOption>
#include <QCommandLineParser>

#include "config/AppConfig.h"
#include "core/SessionManager.h"
#include "terminal/TerminalWidget.h"
#include "ui/MainWindow.h"

namespace svy::app {

Application::Application(int& argc, char** argv)
    : m_qapp(argc, argv) {
    m_qapp.setApplicationName("SVY-Term");
    m_qapp.setOrganizationName("SVY");
#ifdef SVYTERM_VERSION
    m_qapp.setApplicationVersion(QString::fromLatin1(SVYTERM_VERSION));
#else
    m_qapp.setApplicationVersion("0.1.0");
#endif
}

Application::~Application() = default;

Application::CliOptions Application::parseCli() const {
    QCommandLineParser parser;
    parser.setApplicationDescription("SVY-Term macOS remote computing toolbox");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption newTabOption("newtab", "Create tab type at startup (local|ssh)", "type");
    QCommandLineOption execOption("exec", "Execute command in a new local tab", "command");
    QCommandLineOption configOption("config", "Use alternate config file path", "path");
    QCommandLineOption hideTermOption("hideterm", "Start with no default tab");

    parser.addOption(newTabOption);
    parser.addOption(execOption);
    parser.addOption(configOption);
    parser.addOption(hideTermOption);

    parser.process(m_qapp);

    CliOptions options;
    options.newTabType = parser.value(newTabOption);
    options.execCommand = parser.value(execOption);
    options.configPath = parser.value(configOption);
    options.hideTerm = parser.isSet(hideTermOption);
    return options;
}

int Application::run() {
    const CliOptions options = parseCli();

    m_config = std::make_unique<svy::config::AppConfig>(options.configPath);
    m_sessionManager = std::make_unique<svy::core::SessionManager>(m_config.get());
    m_sessionManager->load();

    m_mainWindow = std::make_unique<svy::ui::MainWindow>(m_sessionManager.get());

    if (!options.hideTerm) {
        if (options.newTabType == "ssh") {
            auto profile = m_sessionManager->createDefaultSshSession();
            profile.name = "SSH quick tab";
            profile.host = "example.com";
            profile.username = "user";
            m_mainWindow->createSshTab(profile);
        } else {
            m_mainWindow->createLocalTab("Local");
        }
    }

    if (!options.execCommand.isEmpty()) {
        auto* terminal = m_mainWindow->createLocalTab("Exec");
        terminal->runCommand(options.execCommand);
    }

    m_mainWindow->show();
    return m_qapp.exec();
}

} // namespace svy::app
