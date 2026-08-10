#include "terminal/SshTerminalWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextCursor>
#include <QVBoxLayout>

#include "protocols/SshClient.h"

namespace svy::terminal {

SshTerminalWidget::SshTerminalWidget(const svy::core::SessionProfile& profile, QWidget* parent)
    : QWidget(parent),
      m_profile(profile),
      m_output(new QPlainTextEdit(this)),
      m_client(new svy::protocols::SshClient(this)) {
        m_output->setReadOnly(false);
        m_output->installEventFilter(this);
        m_output->setContextMenuPolicy(Qt::CustomContextMenu);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(m_output);
    setLayout(layout);

    connect(m_client, &svy::protocols::SshClient::outputReceived,
            this, &SshTerminalWidget::onOutputReceived);
    connect(m_client, &svy::protocols::SshClient::errorOccurred,
            this, &SshTerminalWidget::onError);
        connect(m_output, &QPlainTextEdit::selectionChanged, this, &SshTerminalWidget::onSelectionChanged);
        connect(m_output, &QPlainTextEdit::customContextMenuRequested, this, &SshTerminalWidget::showContextMenu);

    append(QString("Connecting to %1@%2:%3 ...")
               .arg(m_profile.username, m_profile.host)
               .arg(m_profile.port));

    if (m_client->connectSession(m_profile)) {
        append("Connected.");
        m_defaultFontSize = m_output->font().pointSize() > 0 ? m_output->font().pointSize() : 12;
        insertPrompt();
    }
}

void SshTerminalWidget::runCommand(const QString& command) {
    executeCommand(command, true);
}

const svy::core::SessionProfile& SshTerminalWidget::profile() const {
    return m_profile;
}

void SshTerminalWidget::adjustFontSize(int delta) {
    QFont f = m_output->font();
    const int current = f.pointSize() > 0 ? f.pointSize() : m_defaultFontSize;
    const int next = qBound(8, current + delta, 36);
    f.setPointSize(next);
    m_output->setFont(f);
}

void SshTerminalWidget::resetFontSize() {
    QFont f = m_output->font();
    f.setPointSize(m_defaultFontSize);
    m_output->setFont(f);
}

bool SshTerminalWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_output || event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }

    auto* keyEvent = static_cast<QKeyEvent*>(event);
    QTextCursor cursor = m_output->textCursor();
    if (cursor.position() < m_commandStart) {
        cursor.movePosition(QTextCursor::End);
        m_output->setTextCursor(cursor);
    }

    if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
        const QString text = m_output->toPlainText();
        const QString command = text.mid(m_commandStart);
        append("\n");
        executeCommand(command, false);
        return true;
    }

    if (keyEvent->key() == Qt::Key_Backspace && m_output->textCursor().position() <= m_commandStart) {
        return true;
    }

    if (keyEvent->key() == Qt::Key_Left && m_output->textCursor().position() <= m_commandStart) {
        return true;
    }

    return QWidget::eventFilter(watched, event);
}

void SshTerminalWidget::executeCommand(const QString& command, bool echoPromptLine) {
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty()) {
        insertPrompt();
        return;
    }

    if (isInteractiveShellCommand(trimmed)) {
        append("[info] interactive shell commands are not supported in SSH command mode.\n");
        append("[hint] run direct remote commands, for example: ls, pwd, cat file.txt\n");
        insertPrompt();
        return;
    }

    if (echoPromptLine) {
        append(QString("$ %1\n").arg(trimmed));
    }
    m_client->execute(trimmed);
}

void SshTerminalWidget::onOutputReceived(const QString& output) {
    append(output);
    if (m_output->toPlainText().endsWith('\n')) {
        insertPrompt();
    }
}

void SshTerminalWidget::onError(const QString& message) {
    append(QString("[error] %1").arg(message));
    insertPrompt();
}

void SshTerminalWidget::append(const QString& line) {
    if (!line.isEmpty()) {
        m_output->moveCursor(QTextCursor::End);
        m_output->insertPlainText(line);
        m_output->moveCursor(QTextCursor::End);
    }
}

void SshTerminalWidget::insertPrompt() {
    const QString all = m_output->toPlainText();
    if (!all.isEmpty() && !all.endsWith('\n')) {
        append("\n");
    }
    append("$ ");
    m_commandStart = m_output->toPlainText().size();
}

void SshTerminalWidget::onSelectionChanged() {
    const QString selected = m_output->textCursor().selectedText();
    if (!selected.isEmpty()) {
        QApplication::clipboard()->setText(selected);
    }
}

void SshTerminalWidget::showContextMenu(const QPoint& pos) {
    QMenu menu(this);
    QAction* copyAction = menu.addAction("Copy");
    QAction* pasteAction = menu.addAction("Paste");
    copyAction->setEnabled(m_output->textCursor().hasSelection());
    pasteAction->setEnabled(!QApplication::clipboard()->text().isEmpty());

    QAction* chosen = menu.exec(m_output->mapToGlobal(pos));
    if (chosen == copyAction) {
        m_output->copy();
        return;
    }

    if (chosen == pasteAction) {
        QTextCursor cursor = m_output->textCursor();
        if (cursor.position() < m_commandStart) {
            cursor.movePosition(QTextCursor::End);
            m_output->setTextCursor(cursor);
        }
        m_output->insertPlainText(QApplication::clipboard()->text());
        m_output->moveCursor(QTextCursor::End);
    }
}

bool SshTerminalWidget::isInteractiveShellCommand(const QString& command) const {
    static const QRegularExpression interactiveRe("^(bash|zsh|sh|fish)\\b");
    return interactiveRe.match(command.trimmed()).hasMatch();
}

} // namespace svy::terminal
