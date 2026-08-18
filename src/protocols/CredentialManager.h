#pragma once

#include <QString>

namespace svy::protocols {

// Secrets live in the macOS Keychain; session config only persists the reference.
class CredentialManager {
public:
    static QString makeReference(const QString& username, const QString& host, int port);

    static bool store(const QString& reference, const QString& secret);
    static QString retrieve(const QString& reference);
    static bool remove(const QString& reference);
    static bool has(const QString& reference);

    static bool isSecureBackendAvailable();

private:
    static QString serviceName();
};

} // namespace svy::protocols
