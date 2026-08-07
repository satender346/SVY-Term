#pragma once

#include <QString>
#include <QVector>

#include "core/SessionTypes.h"

namespace svy::config {

class AppConfig {
public:
    explicit AppConfig(const QString& overrideConfigPath = QString());

    QString configPath() const;
    QString stateDirectory() const;

    QVector<svy::core::SessionProfile> loadSessions() const;
    bool saveSessions(const QVector<svy::core::SessionProfile>& sessions) const;

private:
    QString m_configPath;

    bool ensureStateDirectory() const;
};

} // namespace svy::config
