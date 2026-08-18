#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include "core/SessionTypes.h"

class QJsonObject;

namespace svy::config {

class AppConfig {
public:
    explicit AppConfig(const QString& overrideConfigPath = QString());

    QString configPath() const;
    QString stateDirectory() const;

    QVector<svy::core::SessionProfile> loadSessions() const;
    bool saveSessions(const QVector<svy::core::SessionProfile>& sessions) const;

    QStringList loadFolders() const;
    bool saveFolders(const QStringList& folders) const;

    QHash<QString, bool> loadUiState() const;
    bool saveUiState(const QHash<QString, bool>& state) const;

    QJsonArray loadTunnels() const;
    bool saveTunnels(const QJsonArray& tunnels) const;

private:
    QString m_configPath;

    QJsonObject readRoot() const;
    bool writeRoot(const QJsonObject& root) const;
    bool ensureStateDirectory() const;
};

} // namespace svy::config
