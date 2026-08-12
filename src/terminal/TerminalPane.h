#pragma once

#include <QByteArray>
#include <QStringList>
#include <QWidget>

namespace svy::terminal {

class TerminalSession;
class TerminalView;

// One PTY + one emulator view; local shell and SSH panes differ only in the launched program.
class TerminalPane : public QWidget {
    Q_OBJECT

public:
    explicit TerminalPane(QWidget* parent = nullptr);
    ~TerminalPane() override;

    bool startProcess(const QString& program, const QStringList& arguments);
    void writeInput(const QByteArray& data);
    void runCommand(const QString& command);
    void adjustFontSize(int delta);
    void resetFontSize();
    bool isRunning() const;

signals:
    void inputProduced(const QByteArray& data);
    void sessionEnded(int exitCode);
    void titleChanged(const QString& title);

protected:
    TerminalView* view() const { return m_view; }

private:
    TerminalView* m_view;
    TerminalSession* m_session = nullptr;
};

} // namespace svy::terminal
