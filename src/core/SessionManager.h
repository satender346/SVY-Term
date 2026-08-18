#pragma once

#include <QObject>
#include <QStringList>
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

    QStringList folders() const;
    bool createFolder(const QString& folderPath);
    bool renameFolder(const QString& folderPath, const QString& newName);
    bool deleteFolder(const QString& folderPath, bool deleteSessions);
    bool moveSessionToFolder(const QString& sessionId, const QString& folderPath);
    QString duplicateSession(const QString& sessionId);

    QString resolvePassword(const SessionProfile& profile) const;
    bool rememberPassword(const SessionProfile& profile, const QString& password);
    bool forgetPassword(const QString& sessionId);

    config::AppConfig* config() const { return m_config; }

signals:
    void sessionsChanged();

private:
    void migrateLegacyCredentials();

    config::AppConfig* m_config;
    QVector<SessionProfile> m_sessions;
    QStringList m_folders;
};

} // namespace svy::core
