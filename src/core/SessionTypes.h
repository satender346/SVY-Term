#pragma once

#include <QString>
#include <QJsonObject>

namespace svy::core {

enum class SessionType {
    Local,
    SSH,
    SFTP,
    Tunnel
};

struct TerminalSettings {
    QString fontFamily = "Menlo";
    int fontSize = 12;
    QString charset = "UTF-8";
    bool backspaceSendsCtrlH = false;
    bool warnBeforeMultiLinePaste = true;
    bool trackTerminalActivity = true;
    bool enableLogging = false;
    QString loggingDirectory;
};

struct SessionProfile {
    QString id;
    QString name;
    SessionType type = SessionType::Local;

    QString host;
    int port = 22;
    QString username;
    QString password;
    QString privateKeyPath;

    bool useProxy = false;
    QString proxyHost;
    int proxyPort = 0;
    QString proxyUsername;
    QString proxyPassword;

    QString tunnelMode = "none"; // none|local|remote|dynamic
    int tunnelLocalPort = 0;
    QString tunnelRemoteHost;
    int tunnelRemotePort = 0;

    bool x11Forwarding = false;
    bool compression = false;
    bool keepAlive = true;

    QString startupCommand;
    bool keepOpenAfterCommand = true;

    TerminalSettings terminal;

    QJsonObject toJson() const;
    static SessionProfile fromJson(const QJsonObject& obj);
};

QString sessionTypeToString(SessionType type);
SessionType sessionTypeFromString(const QString& value);

} // namespace svy::core
