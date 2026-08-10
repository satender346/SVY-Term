#include "protocols/CredentialCache.h"

#include <QHash>

namespace svy::protocols {
namespace {

QString makeKey(const QString& username, const QString& host, int port) {
    return QString("%1|%2|%3").arg(username.trimmed(), host.trimmed()).arg(port);
}

QHash<QString, QString>& passwordStore() {
    static QHash<QString, QString> store;
    return store;
}

} // namespace

QString CredentialCache::getPassword(const QString& username, const QString& host, int port) {
    return passwordStore().value(makeKey(username, host, port));
}

void CredentialCache::setPassword(const QString& username, const QString& host, int port, const QString& password) {
    if (!password.isEmpty()) {
        passwordStore().insert(makeKey(username, host, port), password);
    }
}

} // namespace svy::protocols
