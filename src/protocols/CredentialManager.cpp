#include "protocols/CredentialManager.h"

#include <QHash>

#if defined(Q_OS_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#endif

namespace svy::protocols {

namespace {

#if !defined(Q_OS_MACOS)
// Non-macOS builds keep secrets in process memory only; nothing is written to disk.
QHash<QString, QString>& memoryStore() {
    static QHash<QString, QString> store;
    return store;
}
#endif

} // namespace

QString CredentialManager::serviceName() {
    return QStringLiteral("SVY-Term");
}

QString CredentialManager::makeReference(const QString& username, const QString& host, int port) {
    if (host.trimmed().isEmpty()) {
        return {};
    }
    return QString("%1@%2:%3").arg(username.trimmed(), host.trimmed()).arg(port > 0 ? port : 22);
}

bool CredentialManager::isSecureBackendAvailable() {
#if defined(Q_OS_MACOS)
    return true;
#else
    return false;
#endif
}

#if defined(Q_OS_MACOS)

namespace {

CFDataRef toCFData(const QByteArray& bytes) {
    return CFDataCreate(kCFAllocatorDefault,
                        reinterpret_cast<const UInt8*>(bytes.constData()),
                        bytes.size());
}

CFStringRef toCFString(const QByteArray& bytes) {
    return CFStringCreateWithBytes(kCFAllocatorDefault,
                                   reinterpret_cast<const UInt8*>(bytes.constData()),
                                   bytes.size(),
                                   kCFStringEncodingUTF8,
                                   false);
}

CFMutableDictionaryRef baseQuery(const QString& reference) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                             0,
                                                             &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks);
    CFStringRef service = toCFString(QStringLiteral("SVY-Term").toUtf8());
    CFStringRef account = toCFString(reference.toUtf8());

    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, service);
    CFDictionarySetValue(query, kSecAttrAccount, account);

    CFRelease(service);
    CFRelease(account);
    return query;
}

} // namespace

bool CredentialManager::store(const QString& reference, const QString& secret) {
    if (reference.isEmpty()) {
        return false;
    }
    remove(reference);

    CFMutableDictionaryRef query = baseQuery(reference);
    CFDataRef secretData = toCFData(secret.toUtf8());
    CFDictionarySetValue(query, kSecValueData, secretData);
    CFDictionarySetValue(query, kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked);

    const OSStatus status = SecItemAdd(query, nullptr);

    CFRelease(secretData);
    CFRelease(query);
    return status == errSecSuccess;
}

QString CredentialManager::retrieve(const QString& reference) {
    if (reference.isEmpty()) {
        return {};
    }

    CFMutableDictionaryRef query = baseQuery(reference);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

    CFTypeRef result = nullptr;
    const OSStatus status = SecItemCopyMatching(query, &result);
    CFRelease(query);

    if (status != errSecSuccess || result == nullptr) {
        return {};
    }

    CFDataRef data = static_cast<CFDataRef>(result);
    const QByteArray bytes(reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
                           static_cast<int>(CFDataGetLength(data)));
    CFRelease(result);
    return QString::fromUtf8(bytes);
}

bool CredentialManager::remove(const QString& reference) {
    if (reference.isEmpty()) {
        return false;
    }
    CFMutableDictionaryRef query = baseQuery(reference);
    const OSStatus status = SecItemDelete(query);
    CFRelease(query);
    return status == errSecSuccess || status == errSecItemNotFound;
}

bool CredentialManager::has(const QString& reference) {
    return !retrieve(reference).isEmpty();
}

#else

bool CredentialManager::store(const QString& reference, const QString& secret) {
    if (reference.isEmpty()) {
        return false;
    }
    memoryStore().insert(reference, secret);
    return true;
}

QString CredentialManager::retrieve(const QString& reference) {
    return memoryStore().value(reference);
}

bool CredentialManager::remove(const QString& reference) {
    return memoryStore().remove(reference) > 0;
}

bool CredentialManager::has(const QString& reference) {
    return memoryStore().contains(reference);
}

#endif

} // namespace svy::protocols
