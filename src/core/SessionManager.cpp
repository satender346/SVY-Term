#include "core/SessionManager.h"

#include <QDateTime>
#include <QUuid>

#include "config/AppConfig.h"

namespace svy::core {

namespace {
QString makeDefaultName(const QString& prefix) {
    return QString("%1 %2").arg(prefix, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"));
}
}

SessionManager::SessionManager(config::AppConfig* config, QObject* parent)
    : QObject(parent),
      m_config(config) {}

const QVector<SessionProfile>& SessionManager::sessions() const {
    return m_sessions;
}

SessionProfile SessionManager::createDefaultLocalSession() const {
    SessionProfile profile;
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    profile.name = makeDefaultName("Local");
    profile.type = SessionType::Local;
    profile.port = 0;
    return profile;
}

SessionProfile SessionManager::createDefaultSshSession() const {
    SessionProfile profile;
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    profile.name = makeDefaultName("SSH");
    profile.type = SessionType::SSH;
    profile.port = 22;
    profile.keepAlive = true;
    return profile;
}

void SessionManager::load() {
    m_sessions = m_config->loadSessions();
    emit sessionsChanged();
}

bool SessionManager::save() {
    return m_config->saveSessions(m_sessions);
}

bool SessionManager::upsert(const SessionProfile& profile) {
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions[i].id == profile.id) {
            m_sessions[i] = profile;
            emit sessionsChanged();
            return save();
        }
    }

    m_sessions.push_back(profile);
    emit sessionsChanged();
    return save();
}

bool SessionManager::removeById(const QString& id) {
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions[i].id == id) {
            m_sessions.removeAt(i);
            emit sessionsChanged();
            return save();
        }
    }
    return false;
}

SessionProfile SessionManager::findById(const QString& id) const {
    for (const auto& session : m_sessions) {
        if (session.id == id) {
            return session;
        }
    }
    return SessionProfile{};
}

QString sessionTypeToString(SessionType type) {
    switch (type) {
    case SessionType::Local:
        return "local";
    case SessionType::SSH:
        return "ssh";
    case SessionType::SFTP:
        return "sftp";
    case SessionType::Tunnel:
        return "tunnel";
    }
    return "local";
}

SessionType sessionTypeFromString(const QString& value) {
    if (value == "ssh") {
        return SessionType::SSH;
    }
    if (value == "sftp") {
        return SessionType::SFTP;
    }
    if (value == "tunnel") {
        return SessionType::Tunnel;
    }
    return SessionType::Local;
}

QJsonObject SessionProfile::toJson() const {
    QJsonObject out;
    out["id"] = id;
    out["name"] = name;
    out["type"] = sessionTypeToString(type);
    out["host"] = host;
    out["port"] = port;
    out["username"] = username;
    out["privateKeyPath"] = privateKeyPath;
    out["x11Forwarding"] = x11Forwarding;
    out["compression"] = compression;
    out["keepAlive"] = keepAlive;
    out["startupCommand"] = startupCommand;
    out["keepOpenAfterCommand"] = keepOpenAfterCommand;

    QJsonObject terminalObj;
    terminalObj["fontFamily"] = terminal.fontFamily;
    terminalObj["fontSize"] = terminal.fontSize;
    terminalObj["charset"] = terminal.charset;
    terminalObj["backspaceSendsCtrlH"] = terminal.backspaceSendsCtrlH;
    terminalObj["warnBeforeMultiLinePaste"] = terminal.warnBeforeMultiLinePaste;
    terminalObj["trackTerminalActivity"] = terminal.trackTerminalActivity;
    terminalObj["enableLogging"] = terminal.enableLogging;
    terminalObj["loggingDirectory"] = terminal.loggingDirectory;

    out["terminal"] = terminalObj;
    return out;
}

SessionProfile SessionProfile::fromJson(const QJsonObject& obj) {
    SessionProfile profile;
    profile.id = obj.value("id").toString();
    profile.name = obj.value("name").toString();
    profile.type = sessionTypeFromString(obj.value("type").toString());
    profile.host = obj.value("host").toString();
    profile.port = obj.value("port").toInt(22);
    profile.username = obj.value("username").toString();
    profile.privateKeyPath = obj.value("privateKeyPath").toString();
    profile.x11Forwarding = obj.value("x11Forwarding").toBool(false);
    profile.compression = obj.value("compression").toBool(false);
    profile.keepAlive = obj.value("keepAlive").toBool(true);
    profile.startupCommand = obj.value("startupCommand").toString();
    profile.keepOpenAfterCommand = obj.value("keepOpenAfterCommand").toBool(true);

    const QJsonObject terminalObj = obj.value("terminal").toObject();
    profile.terminal.fontFamily = terminalObj.value("fontFamily").toString("Menlo");
    profile.terminal.fontSize = terminalObj.value("fontSize").toInt(12);
    profile.terminal.charset = terminalObj.value("charset").toString("UTF-8");
    profile.terminal.backspaceSendsCtrlH = terminalObj.value("backspaceSendsCtrlH").toBool(false);
    profile.terminal.warnBeforeMultiLinePaste = terminalObj.value("warnBeforeMultiLinePaste").toBool(true);
    profile.terminal.trackTerminalActivity = terminalObj.value("trackTerminalActivity").toBool(true);
    profile.terminal.enableLogging = terminalObj.value("enableLogging").toBool(false);
    profile.terminal.loggingDirectory = terminalObj.value("loggingDirectory").toString();

    return profile;
}

} // namespace svy::core
