#include "terminal/PtySession.h"

#include <QCoreApplication>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

namespace svy::terminal {

PtySession::PtySession(QObject* parent)
    : TerminalSession(parent) {}

PtySession::~PtySession() {
    closeSession();
}

bool PtySession::start(const QString& program, const QStringList& arguments, int columns, int rows) {
    if (m_running) {
        return false;
    }

    struct winsize ws {};
    ws.ws_col = static_cast<unsigned short>(qMax(columns, 1));
    ws.ws_row = static_cast<unsigned short>(qMax(rows, 1));

    int master = -1;
    const pid_t pid = forkpty(&master, nullptr, nullptr, &ws);
    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
        setsid();
        ::signal(SIGINT, SIG_DFL);
        ::signal(SIGQUIT, SIG_DFL);
        ::signal(SIGTSTP, SIG_DFL);
        ::signal(SIGPIPE, SIG_DFL);

        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        unsetenv("LINES");
        unsetenv("COLUMNS");

        QList<QByteArray> storage;
        storage.append(program.toLocal8Bit());
        for (const QString& argument : arguments) {
            storage.append(argument.toLocal8Bit());
        }

        std::vector<char*> argv;
        argv.reserve(storage.size() + 1);
        for (QByteArray& item : storage) {
            argv.push_back(item.data());
        }
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        _exit(127);
    }

    m_master = master;
    m_child = pid;
    m_running = true;

    const int flags = fcntl(m_master, F_GETFL, 0);
    fcntl(m_master, F_SETFL, flags | O_NONBLOCK);

    m_notifier = new QSocketNotifier(m_master, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &PtySession::onMasterReadable);
    m_notifier->setEnabled(true);
    return true;
}

bool PtySession::isRunning() const {
    return m_running;
}

void PtySession::write(const QByteArray& data) {
    if (!m_running || m_master < 0 || data.isEmpty()) {
        return;
    }

    qint64 written = 0;
    while (written < data.size()) {
        const ssize_t result = ::write(m_master, data.constData() + written, data.size() - written);
        if (result > 0) {
            written += result;
            continue;
        }
        if (result < 0 && (errno == EAGAIN || errno == EINTR)) {
            continue;
        }
        break;
    }
}

void PtySession::resize(int columns, int rows) {
    if (!m_running || m_master < 0) {
        return;
    }

    struct winsize ws {};
    ws.ws_col = static_cast<unsigned short>(qMax(columns, 1));
    ws.ws_row = static_cast<unsigned short>(qMax(rows, 1));
    ioctl(m_master, TIOCSWINSZ, &ws);
}

void PtySession::onMasterReadable() {
    char buffer[16384];
    for (int pass = 0; pass < 32; ++pass) {
        const ssize_t count = ::read(m_master, buffer, sizeof(buffer));
        if (count > 0) {
            emit dataReceived(QByteArray(buffer, static_cast<int>(count)));
            continue;
        }

        if (count == 0) {
            teardown(0);
            return;
        }

        if (errno == EAGAIN) {
            return;
        }
        if (errno == EINTR) {
            continue;
        }

        teardown(0);
        return;
    }
}

void PtySession::closeSession() {
    if (!m_running) {
        return;
    }
    if (m_child > 0) {
        ::kill(m_child, SIGHUP);
    }
    teardown(0);
}

void PtySession::teardown(int exitCode) {
    if (!m_running) {
        return;
    }
    m_running = false;

    if (m_notifier != nullptr) {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }

    if (m_master >= 0) {
        ::close(m_master);
        m_master = -1;
    }

    if (m_child > 0) {
        int status = 0;
        if (waitpid(m_child, &status, WNOHANG) == m_child && WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
        } else {
            ::kill(m_child, SIGKILL);
            waitpid(m_child, &status, 0);
        }
        m_child = -1;
    }

    emit sessionEnded(exitCode);
}

} // namespace svy::terminal
