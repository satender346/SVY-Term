#pragma once

#include <QByteArray>
#include <QObject>

namespace svy::terminal {

// Transport-agnostic terminal backend: local PTY today, any stream tomorrow.
class TerminalSession : public QObject {
    Q_OBJECT

public:
    explicit TerminalSession(QObject* parent = nullptr) : QObject(parent) {}
    ~TerminalSession() override = default;

    virtual bool isRunning() const = 0;
    virtual void write(const QByteArray& data) = 0;
    virtual void resize(int columns, int rows) = 0;
    virtual void closeSession() = 0;

signals:
    void dataReceived(const QByteArray& data);
    void sessionEnded(int exitCode);
};

} // namespace svy::terminal
