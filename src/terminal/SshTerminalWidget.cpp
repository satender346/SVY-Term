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

namespace {

QString normalizeTerminalOutput(const QString& raw) {
    QString out = raw;

    out.replace("\r\n", "\n");
    out.replace('\r', '\n');

    // Remove ANSI CSI sequences (colors, cursor moves, bracketed paste toggles, etc.).
    static const QRegularExpression csiPattern("\\x1B\\[[0-?]*[ -/]*[@-~]");
    out.remove(csiPattern);

    // Remove OSC sequences.
    static const QRegularExpression oscPattern("\\x1B\\][^\\x07\\x1B]*(\\x07|\\x1B\\\\)");
    out.remove(oscPattern);

    // Remove remaining C1/control sequences except newlines and tabs.
    static const QRegularExpression controlPattern("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F]");
    out.remove(controlPattern);

    return out;
}

} // namespace

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

SshTerminalWidget::~SshTerminalWidget() {
    if (m_client != nullptr) {
        disconnect(m_client, nullptr, this, nullptr);
        m_client->blockSignals(true);
    }
}

void SshTerminalWidget::runCommand(const QString& command) {
    executeCommand(command, true);
}

const svy::core::SessionProfile& SshTerminalWidget::profile() const {
    return m_profile;
}

void SshTerminalWidget::adjustFontSize(int delta) {
    if (m_output.isNull()) {
        return;
    }
    QFont f = m_output->font();
    const int current = f.pointSize() > 0 ? f.pointSize() : m_defaultFontSize;
    const int next = qBound(8, current + delta, 36);
    f.setPointSize(next);
    m_output->setFont(f);
}

void SshTerminalWidget::resetFontSize() {
    if (m_output.isNull()) {
        return;
    }
    QFont f = m_output->font();
    f.setPointSize(m_defaultFontSize);
    m_output->setFont(f);
}

bool SshTerminalWidget::eventFilter(QObject* watched, QEvent* event) {
    if (m_output.isNull()) {
        return QWidget::eventFilter(watched, event);
    }
    if (watched != m_output || event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }

    auto* keyEvent = static_cast<QKeyEvent*>(event);

    if (m_passwordInputMode) {
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            append("\n");
            m_client->execute(m_hiddenInputBuffer);
            m_hiddenInputBuffer.clear();
            m_passwordInputMode = false;
            return true;
        }

        if (keyEvent->key() == Qt::Key_Backspace) {
            if (!m_hiddenInputBuffer.isEmpty()) {
                m_hiddenInputBuffer.chop(1);
            }
            return true;
        }

        if (keyEvent->matches(QKeySequence::Paste)) {
            m_hiddenInputBuffer += QApplication::clipboard()->text();
            return true;
        }

        const QString typed = keyEvent->text();
        if (!typed.isEmpty() && typed.at(0).isPrint()) {
            m_hiddenInputBuffer += typed;
            return true;
        }
        return true;
    }

    QTextCursor cursor = m_output->textCursor();
    if (cursor.position() < m_commandStart) {
        cursor.movePosition(QTextCursor::End);
        m_output->setTextCursor(cursor);
    }

    if (keyEvent->matches(QKeySequence::Paste)) {
        pasteAtCommand(QApplication::clipboard()->text());
        return true;
    }

    if (keyEvent->matches(QKeySequence::Copy)) {
        m_output->copy();
        return true;
    }

    if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
        const QString command = currentCommandText();
        if (!command.trimmed().isEmpty()) {
            m_history.removeAll(command.trimmed());
            m_history.append(command.trimmed());
            emit commandEntered(command.trimmed());
        }
        m_historyIndex = m_history.size();
        append("\n");
        executeCommand(command, false);
        return true;
    }

    if (keyEvent->key() == Qt::Key_Up) {
        recallHistory(-1);
        return true;
    }

    if (keyEvent->key() == Qt::Key_Down) {
        recallHistory(1);
        return true;
    }

    if (keyEvent->key() == Qt::Key_Home ||
        (keyEvent->key() == Qt::Key_A && keyEvent->modifiers().testFlag(Qt::ControlModifier))) {
        QTextCursor c = m_output->textCursor();
        c.setPosition(m_commandStart);
        m_output->setTextCursor(c);
        return true;
    }

    if (keyEvent->key() == Qt::Key_E && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
        m_output->moveCursor(QTextCursor::End);
        return true;
    }

    if (keyEvent->key() == Qt::Key_U && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
        replaceCurrentCommand(QString());
        return true;
    }

    if (keyEvent->key() == Qt::Key_L && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
        m_output->clear();
        m_commandStart = 0;
        return true;
    }

    if (keyEvent->key() == Qt::Key_C && keyEvent->modifiers().testFlag(Qt::ControlModifier) &&
        !m_output->textCursor().hasSelection()) {
        replaceCurrentCommand(QString());
        append("^C\n");
        m_client->execute(QString());
        insertPrompt();
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

QString SshTerminalWidget::currentCommandText() const {
    if (m_output.isNull()) {
        return {};
    }
    return m_output->toPlainText().mid(m_commandStart);
}

void SshTerminalWidget::replaceCurrentCommand(const QString& text) {
    if (m_output.isNull()) {
        return;
    }
    QTextCursor cursor = m_output->textCursor();
    cursor.setPosition(m_commandStart);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.insertText(text);
    m_output->setTextCursor(cursor);
    m_output->moveCursor(QTextCursor::End);
}

void SshTerminalWidget::pasteAtCommand(const QString& text) {
    if (m_output.isNull() || text.isEmpty()) {
        return;
    }

    if (m_passwordInputMode) {
        m_hiddenInputBuffer += text;
        return;
    }

    QString sanitized = text;
    sanitized.replace("\r\n", "\n");
    sanitized.replace('\r', '\n');
    if (sanitized.endsWith('\n')) {
        sanitized.chop(1);
    }
    sanitized.replace('\n', ' ');

    QTextCursor cursor = m_output->textCursor();
    if (cursor.position() < m_commandStart) {
        cursor.movePosition(QTextCursor::End);
    }
    cursor.insertText(sanitized);
    m_output->setTextCursor(cursor);
    m_output->moveCursor(QTextCursor::End);
}

void SshTerminalWidget::recallHistory(int direction) {
    if (m_history.isEmpty()) {
        return;
    }

    int next = m_historyIndex + direction;
    next = qBound(0, next, m_history.size());
    m_historyIndex = next;
    replaceCurrentCommand(next < m_history.size() ? m_history.at(next) : QString());
}

void SshTerminalWidget::executeCommand(const QString& command, bool echoPromptLine) {
    Q_UNUSED(echoPromptLine);
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    static const QRegularExpression sshCommandPrefix("^ssh\\s+");
    if (sshCommandPrefix.match(trimmed).hasMatch()) {
        emit sshCommandRequested(trimmed);
        return;
    }

    if (trimmed == "clear") {
        m_output->clear();
        m_commandStart = 0;
        return;
    }

    m_client->execute(trimmed);
}

void SshTerminalWidget::onOutputReceived(const QString& output) {
    if (m_output.isNull()) {
        return;
    }
    const QString normalized = normalizeTerminalOutput(output);
    append(normalized);

    static const QRegularExpression passwordPrompt(
        "(^|\\n)(\\[sudo\\]\\s*)?(password|passphrase)[^\\n]*:\\s*$",
        QRegularExpression::CaseInsensitiveOption);
    if (passwordPrompt.match(normalized).hasMatch()) {
        m_passwordInputMode = true;
        m_hiddenInputBuffer.clear();
    }

    insertPrompt();
}

void SshTerminalWidget::onError(const QString& message) {
    if (m_output.isNull()) {
        return;
    }
    append(QString("[error] %1\n").arg(message));
    insertPrompt();
}

void SshTerminalWidget::append(const QString& line) {
    if (!line.isEmpty() && !m_output.isNull()) {
        m_output->moveCursor(QTextCursor::End);
        m_output->insertPlainText(line);
        m_output->moveCursor(QTextCursor::End);
    }
}

void SshTerminalWidget::insertPrompt() {
    if (m_output.isNull()) {
        return;
    }
    m_commandStart = m_output->toPlainText().size();
}

void SshTerminalWidget::onSelectionChanged() {
    if (m_output.isNull()) {
        return;
    }
    const QString selected = m_output->textCursor().selectedText();
    if (!selected.isEmpty() && QApplication::clipboard()->supportsSelection()) {
        QApplication::clipboard()->setText(selected, QClipboard::Selection);
    }
}

void SshTerminalWidget::showContextMenu(const QPoint& pos) {
    if (m_output.isNull()) {
        return;
    }
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
        pasteAtCommand(QApplication::clipboard()->text());
    }
}

bool SshTerminalWidget::isInteractiveShellCommand(const QString& command) const {
    Q_UNUSED(command);
    return false;
}

} // namespace svy::terminal
