#pragma once

#include <QObject>
#include <QPointer>
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

    svy::core::SessionProfile m_profile;
    QPointer<QPlainTextEdit> m_output;
    svy::protocols::SshClient* m_client;
    int m_commandStart = 0;
    int m_defaultFontSize = 12;
    QString m_promptPrefix;
};

} // namespace svy::terminal
