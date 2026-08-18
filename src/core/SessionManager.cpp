#include "core/SessionManager.h"

#include <QDateTime>
#include <QUuid>

#include "config/AppConfig.h"
#include "protocols/CredentialManager.h"

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
    profile.name = "";
    profile.type = SessionType::SSH;
    profile.port = 22;
    profile.keepAlive = true;
    return profile;
}

void SessionManager::load() {
    m_sessions = m_config->loadSessions();
    m_folders = m_config->loadFolders();
    migrateLegacyCredentials();
    emit sessionsChanged();
}

bool SessionManager::save() {
    return m_config->saveSessions(m_sessions) && m_config->saveFolders(m_folders);
}

void SessionManager::migrateLegacyCredentials() {
    bool migrated = false;
    for (auto& session : m_sessions) {
        if (session.password.isEmpty()) {
            continue;
        }

        // Old configs kept the password in JSON; move it into the Keychain and strip it.
        const QString reference = session.credentialRef.isEmpty()
                                      ? protocols::CredentialManager::makeReference(session.username, session.host, session.port)
                                      : session.credentialRef;
        if (!reference.isEmpty() && protocols::CredentialManager::store(reference, session.password)) {
            session.credentialRef = reference;
            session.rememberCredentials = true;
        }
        session.password.clear();
        migrated = true;
    }

    if (migrated) {
        save();
    }
}

QStringList SessionManager::folders() const {
    QStringList all = m_folders;
    for (const auto& session : m_sessions) {
        if (!session.folderPath.isEmpty() && !all.contains(session.folderPath)) {
            all.append(session.folderPath);
        }
    }
    all.sort();
    return all;
}

bool SessionManager::createFolder(const QString& folderPath) {
    const QString trimmed = folderPath.trimmed();
    if (trimmed.isEmpty() || m_folders.contains(trimmed)) {
        return false;
    }
    m_folders.append(trimmed);
    emit sessionsChanged();
    return save();
}

bool SessionManager::renameFolder(const QString& folderPath, const QString& newName) {
    const QString trimmed = newName.trimmed();
    if (folderPath.isEmpty() || trimmed.isEmpty()) {
        return false;
    }

    const int lastSeparator = folderPath.lastIndexOf('/');
    const QString parent = lastSeparator > 0 ? folderPath.left(lastSeparator) : QString();
    const QString updated = parent.isEmpty() ? trimmed : parent + "/" + trimmed;
    if (updated == folderPath) {
        return true;
    }

    for (auto& folder : m_folders) {
        if (folder == folderPath) {
            folder = updated;
        } else if (folder.startsWith(folderPath + "/")) {
            folder = updated + folder.mid(folderPath.size());
        }
    }

    for (auto& session : m_sessions) {
        if (session.folderPath == folderPath) {
            session.folderPath = updated;
        } else if (session.folderPath.startsWith(folderPath + "/")) {
            session.folderPath = updated + session.folderPath.mid(folderPath.size());
        }
    }

    emit sessionsChanged();
    return save();
}

bool SessionManager::deleteFolder(const QString& folderPath, bool deleteSessions) {
    if (folderPath.isEmpty()) {
        return false;
    }

    m_folders.removeAll(folderPath);
    for (int i = m_folders.size() - 1; i >= 0; --i) {
        if (m_folders.at(i).startsWith(folderPath + "/")) {
            m_folders.removeAt(i);
        }
    }

    for (int i = m_sessions.size() - 1; i >= 0; --i) {
        const QString sessionFolder = m_sessions.at(i).folderPath;
        if (sessionFolder != folderPath && !sessionFolder.startsWith(folderPath + "/")) {
            continue;
        }
        if (deleteSessions) {
            forgetPassword(m_sessions.at(i).id);
            m_sessions.removeAt(i);
        } else {
            m_sessions[i].folderPath.clear();
        }
    }

    emit sessionsChanged();
    return save();
}

bool SessionManager::moveSessionToFolder(const QString& sessionId, const QString& folderPath) {
    for (auto& session : m_sessions) {
        if (session.id != sessionId) {
            continue;
        }
        session.folderPath = folderPath;
        emit sessionsChanged();
        return save();
    }
    return false;
}

QString SessionManager::duplicateSession(const QString& sessionId) {
    for (const auto& session : m_sessions) {
        if (session.id != sessionId) {
            continue;
        }

        SessionProfile copy = session;
        copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        copy.name = session.name + " (copy)";
        copy.password.clear();

        // The duplicate points at the same Keychain entry, so no re-prompt is needed.
        m_sessions.push_back(copy);
        emit sessionsChanged();
        save();
        return copy.id;
    }
    return {};
}

QString SessionManager::resolvePassword(const SessionProfile& profile) const {
    if (!profile.password.isEmpty()) {
        return profile.password;
    }

    const QString reference = profile.credentialRef.isEmpty()
                                  ? protocols::CredentialManager::makeReference(profile.username, profile.host, profile.port)
                                  : profile.credentialRef;
    return protocols::CredentialManager::retrieve(reference);
}

bool SessionManager::rememberPassword(const SessionProfile& profile, const QString& password) {
    if (password.isEmpty()) {
        return false;
    }

    const QString reference = protocols::CredentialManager::makeReference(profile.username, profile.host, profile.port);
    if (reference.isEmpty() || !protocols::CredentialManager::store(reference, password)) {
        return false;
    }

    for (auto& session : m_sessions) {
        if (session.id != profile.id) {
            continue;
        }
        session.credentialRef = reference;
        session.rememberCredentials = true;
        session.password.clear();
        emit sessionsChanged();
        return save();
    }
    return true;
}

bool SessionManager::forgetPassword(const QString& sessionId) {
    for (auto& session : m_sessions) {
        if (session.id != sessionId) {
            continue;
        }

        const QString reference = session.credentialRef.isEmpty()
                                      ? protocols::CredentialManager::makeReference(session.username, session.host, session.port)
                                      : session.credentialRef;
        protocols::CredentialManager::remove(reference);
        session.credentialRef.clear();
        session.rememberCredentials = false;
        session.password.clear();
        emit sessionsChanged();
        return save();
    }
    return false;
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
            forgetPassword(id);
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
    out["folderPath"] = folderPath;
    out["sortOrder"] = sortOrder;
    out["host"] = host;
    out["port"] = port;
    out["username"] = username;
    out["authMethod"] = authMethod;
    out["rememberCredentials"] = rememberCredentials;
    out["credentialRef"] = credentialRef;
    out["privateKeyPath"] = privateKeyPath;
    out["useProxy"] = useProxy;
    out["proxyHost"] = proxyHost;
    out["proxyPort"] = proxyPort;
    out["proxyUsername"] = proxyUsername;
    out["tunnelMode"] = tunnelMode;
    out["tunnelLocalPort"] = tunnelLocalPort;
    out["tunnelRemoteHost"] = tunnelRemoteHost;
    out["tunnelRemotePort"] = tunnelRemotePort;
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
    profile.folderPath = obj.value("folderPath").toString();
    profile.sortOrder = obj.value("sortOrder").toInt(0);
    profile.host = obj.value("host").toString();
    profile.port = obj.value("port").toInt(22);
    profile.username = obj.value("username").toString();
    // Legacy configs stored the password inline; it is migrated to the Keychain on load.
    profile.password = obj.value("password").toString();
    profile.authMethod = obj.value("authMethod").toString("password");
    profile.rememberCredentials = obj.value("rememberCredentials").toBool(false);
    profile.credentialRef = obj.value("credentialRef").toString();
    profile.privateKeyPath = obj.value("privateKeyPath").toString();
    profile.useProxy = obj.value("useProxy").toBool(false);
    profile.proxyHost = obj.value("proxyHost").toString();
    profile.proxyPort = obj.value("proxyPort").toInt(0);
    profile.proxyUsername = obj.value("proxyUsername").toString();
    profile.tunnelMode = obj.value("tunnelMode").toString("none");
    profile.tunnelLocalPort = obj.value("tunnelLocalPort").toInt(0);
    profile.tunnelRemoteHost = obj.value("tunnelRemoteHost").toString();
    profile.tunnelRemotePort = obj.value("tunnelRemotePort").toInt(0);
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
