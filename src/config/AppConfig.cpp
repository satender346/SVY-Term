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
    if (!ensureStateDirectory()) {
        return false;
    }

    QJsonArray items;
    for (const auto& session : sessions) {
        items.push_back(session.toJson());
    }

    QJsonObject root;
    root["sessions"] = items;

    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace svy::config
