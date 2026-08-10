#pragma once

#include <QString>

namespace svy::protocols {

class CredentialCache {
public:
    static QString getPassword(const QString& username, const QString& host, int port);
    static void setPassword(const QString& username, const QString& host, int port, const QString& password);
};

} // namespace svy::protocols
