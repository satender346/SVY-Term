#include "config/AppConfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace svy::config {

namespace {
QString defaultConfigPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(base).filePath("svy-term/svy-term.json");
}
}

AppConfig::AppConfig(const QString& overrideConfigPath)
    : m_configPath(overrideConfigPath.isEmpty() ? defaultConfigPath() : overrideConfigPath) {}

QString AppConfig::configPath() const {
    return m_configPath;
}

QString AppConfig::stateDirectory() const {
    return QFileInfo(m_configPath).absolutePath();
}

bool AppConfig::ensureStateDirectory() const {
    QDir dir(stateDirectory());
    if (dir.exists()) {
        return true;
    }
    return dir.mkpath(".");
}

QVector<svy::core::SessionProfile> AppConfig::loadSessions() const {
    QVector<svy::core::SessionProfile> sessions;
    QFile file(m_configPath);

    if (!file.exists()) {
        return sessions;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return sessions;
    }

    const QByteArray content = file.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(content);
    const QJsonObject root = doc.object();
    const QJsonArray items = root.value("sessions").toArray();

    sessions.reserve(items.size());
    for (const auto& item : items) {
        sessions.push_back(svy::core::SessionProfile::fromJson(item.toObject()));
    }

    return sessions;
}

bool AppConfig::saveSessions(const QVector<svy::core::SessionProfile>& sessions) const {
    QJsonArray items;
    for (const auto& session : sessions) {
        items.push_back(session.toJson());
    }

    QJsonObject root = readRoot();
    root["sessions"] = items;
    return writeRoot(root);
}

QJsonObject AppConfig::readRoot() const {
    QFile file(m_configPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool AppConfig::writeRoot(const QJsonObject& root) const {
    if (!ensureStateDirectory()) {
        return false;
    }

    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    return true;
}

QStringList AppConfig::loadFolders() const {
    QStringList folders;
    const QJsonArray items = readRoot().value("folders").toArray();
    for (const auto& item : items) {
        const QString value = item.toString();
        if (!value.isEmpty()) {
            folders.append(value);
        }
    }
    return folders;
}

bool AppConfig::saveFolders(const QStringList& folders) const {
    QJsonArray items;
    for (const QString& folder : folders) {
        items.push_back(folder);
    }

    QJsonObject root = readRoot();
    root["folders"] = items;
    return writeRoot(root);
}

QHash<QString, bool> AppConfig::loadUiState() const {
    QHash<QString, bool> state;
    const QJsonObject obj = readRoot().value("ui").toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        state.insert(it.key(), it.value().toBool());
    }
    return state;
}

bool AppConfig::saveUiState(const QHash<QString, bool>& state) const {
    QJsonObject obj;
    for (auto it = state.begin(); it != state.end(); ++it) {
        obj[it.key()] = it.value();
    }

    QJsonObject root = readRoot();
    root["ui"] = obj;
    return writeRoot(root);
}

QJsonArray AppConfig::loadTunnels() const {
    return readRoot().value("tunnels").toArray();
}

bool AppConfig::saveTunnels(const QJsonArray& tunnels) const {
    QJsonObject root = readRoot();
    root["tunnels"] = tunnels;
    return writeRoot(root);
}

} // namespace svy::config
