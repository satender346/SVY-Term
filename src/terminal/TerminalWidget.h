#pragma once

#include <QObject>
#include <QProcess>
#include <QWidget>

class QEvent;
class QKeyEvent;
class QPlainTextEdit;

namespace svy::terminal {

class TerminalWidget : public QWidget {
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget* parent = nullptr);

    void runCommand(const QString& command);
    bool isBusy() const;

signals:
    void titleSuggested(const QString& title);

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessStateChanged(QProcess::ProcessState state);

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void appendOutput(const QString& text);
    void appendStatus(const QString& text);
    void insertPrompt();
    void executeCommand(const QString& command, bool echoPromptLine);

    QPlainTextEdit* m_output;
    QProcess* m_process;
    int m_commandStart = 0;
};

} // namespace svy::terminal
