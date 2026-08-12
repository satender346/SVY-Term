#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "core/SessionTypes.h"

class QEvent;
class QPlainTextEdit;
class QPoint;

namespace svy::protocols {
class SshClient;
}

namespace svy::terminal {

class SshTerminalWidget : public QWidget {
    Q_OBJECT

public:
    explicit SshTerminalWidget(const svy::core::SessionProfile& profile, QWidget* parent = nullptr);
    ~SshTerminalWidget() override;
    void runCommand(const QString& command);
    const svy::core::SessionProfile& profile() const;
    void adjustFontSize(int delta);
    void resetFontSize();

signals:
    void sshCommandRequested(const QString& command);
    void commandEntered(const QString& command);

private slots:
    void onOutputReceived(const QString& output);
    void onError(const QString& message);
    void onSelectionChanged();
    void showContextMenu(const QPoint& pos);

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool isInteractiveShellCommand(const QString& command) const;
    void append(const QString& line);
    void insertPrompt();
    void executeCommand(const QString& command, bool echoPromptLine);
    QString currentCommandText() const;
    void replaceCurrentCommand(const QString& text);
    void pasteAtCommand(const QString& text);
    void recallHistory(int direction);

    svy::core::SessionProfile m_profile;
    QPointer<QPlainTextEdit> m_output;
    svy::protocols::SshClient* m_client;
    int m_commandStart = 0;
    int m_defaultFontSize = 12;
    bool m_passwordInputMode = false;
    QString m_hiddenInputBuffer;
    QStringList m_history;
    int m_historyIndex = 0;
};

} // namespace svy::terminal
