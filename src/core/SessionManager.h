#pragma once

#include <QObject>
#include <QVector>

#include "core/SessionTypes.h"

namespace svy::config {
class AppConfig;
}

namespace svy::core {

class SessionManager : public QObject {
    Q_OBJECT

public:
    explicit SessionManager(config::AppConfig* config, QObject* parent = nullptr);

    const QVector<SessionProfile>& sessions() const;
    SessionProfile createDefaultLocalSession() const;
    SessionProfile createDefaultSshSession() const;

    void load();
    bool save();

    bool upsert(const SessionProfile& profile);
    bool removeById(const QString& id);
    SessionProfile findById(const QString& id) const;

signals:
    void sessionsChanged();

private:
    config::AppConfig* m_config;
    QVector<SessionProfile> m_sessions;
};

} // namespace svy::core
