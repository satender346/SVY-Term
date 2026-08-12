#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QWidget>

class QEvent;
class QKeyEvent;
class QPlainTextEdit;
class QPoint;

namespace svy::terminal {

class TerminalWidget : public QWidget {
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget* parent = nullptr);

    void runCommand(const QString& command);
    bool isBusy() const;
    void adjustFontSize(int delta);
    void resetFontSize();

signals:
    void titleSuggested(const QString& title);
    void sshCommandRequested(const QString& command);
    void commandEntered(const QString& command);

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessStateChanged(QProcess::ProcessState state);
    void onSelectionChanged();
    void showContextMenu(const QPoint& pos);

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    QString sanitizeCommand(const QString& command) const;
    bool isInteractiveShellCommand(const QString& command) const;
    void appendOutput(const QString& text);
    void appendStatus(const QString& text);
    void insertPrompt();
    void executeCommand(const QString& command, bool echoPromptLine);
    QString currentCommandText() const;
    void replaceCurrentCommand(const QString& text);
    void pasteAtCommand(const QString& text);
    void recallHistory(int direction);

    QPlainTextEdit* m_output;
    QProcess* m_process;
    int m_commandStart = 0;
    int m_defaultFontSize = 12;
    QStringList m_history;
    int m_historyIndex = 0;
};

} // namespace svy::terminal
