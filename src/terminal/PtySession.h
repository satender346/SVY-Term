#pragma once

#include <QSocketNotifier>
#include <QStringList>

#include <sys/types.h>

#include "terminal/TerminalSession.h"

namespace svy::terminal {

class PtySession : public TerminalSession {
    Q_OBJECT

public:
    explicit PtySession(QObject* parent = nullptr);
    ~PtySession() override;

    bool start(const QString& program, const QStringList& arguments, int columns, int rows);

    bool isRunning() const override;
    void write(const QByteArray& data) override;
    void resize(int columns, int rows) override;
    void closeSession() override;

private slots:
    void onMasterReadable();

private:
    void teardown(int exitCode);

    int m_master = -1;
    pid_t m_child = -1;
    QSocketNotifier* m_notifier = nullptr;
    bool m_running = false;
};

} // namespace svy::terminal
