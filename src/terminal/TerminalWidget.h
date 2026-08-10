#pragma once

#include <QProcess>
#include <QWidget>

class QLineEdit;
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
    void onEnterPressed();
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessStateChanged(QProcess::ProcessState state);

private:
    void appendOutput(const QString& text);

    QPlainTextEdit* m_output;
    QLineEdit* m_input;
    QProcess* m_process;
};

} // namespace svy::terminal
