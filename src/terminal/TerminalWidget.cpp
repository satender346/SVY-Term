#include "terminal/TerminalWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextCursor>
#include <QVBoxLayout>

namespace svy::terminal {

namespace {

QString normalizeTerminalOutput(const QString& raw) {
    QString out = raw;

    out.replace("\r\n", "\n");
    out.replace('\r', '\n');

    static const QRegularExpression csiPattern("\\x1B\\[[0-?]*[ -/]*[@-~]");
    out.remove(csiPattern);

    static const QRegularExpression oscPattern("\\x1B\\][^\\x07\\x1B]*(\\x07|\\x1B\\\\)");
    out.remove(oscPattern);

    static const QRegularExpression controlPattern("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F]");
    out.remove(controlPattern);

    return out;
}

} // namespace

TerminalWidget::TerminalWidget(QWidget* parent)
    : QWidget(parent),
      m_output(new QPlainTextEdit(this)),
      m_process(new QProcess(this)) {
    m_output->setReadOnly(false);
    m_output->setObjectName("terminalOutput");
    m_output->installEventFilter(this);
    m_output->setContextMenuPolicy(Qt::CustomContextMenu);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(m_output);
    setLayout(layout);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &TerminalWidget::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &TerminalWidget::onReadyReadStderr);
    connect(m_process, &QProcess::stateChanged, this, &TerminalWidget::onProcessStateChanged);
    connect(m_output, &QPlainTextEdit::selectionChanged, this, &TerminalWidget::onSelectionChanged);
    connect(m_output, &QPlainTextEdit::customContextMenuRequested, this, &TerminalWidget::showContextMenu);

    appendStatus("[ready]");
    m_defaultFontSize = m_output->font().pointSize() > 0 ? m_output->font().pointSize() : 12;
    insertPrompt();
}

void TerminalWidget::runCommand(const QString& command) {
    executeCommand(command, true);
}

bool TerminalWidget::isBusy() const {
    return m_process->state() != QProcess::NotRunning;
}

void TerminalWidget::adjustFontSize(int delta) {
    QFont f = m_output->font();
    const int current = f.pointSize() > 0 ? f.pointSize() : m_defaultFontSize;
    const int next = qBound(8, current + delta, 36);
    f.setPointSize(next);
    m_output->setFont(f);
}

void TerminalWidget::resetFontSize() {
    QFont f = m_output->font();
    f.setPointSize(m_defaultFontSize);
    m_output->setFont(f);
}

bool TerminalWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_output || event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }

    auto* keyEvent = static_cast<QKeyEvent*>(event);
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
        insertPrompt();
        return true;
    }

    if (keyEvent->key() == Qt::Key_C && keyEvent->modifiers().testFlag(Qt::ControlModifier) &&
        !m_output->textCursor().hasSelection()) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
        }
        appendOutput("^C\n");
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

QString TerminalWidget::currentCommandText() const {
    return m_output->toPlainText().mid(m_commandStart);
}

void TerminalWidget::replaceCurrentCommand(const QString& text) {
    QTextCursor cursor = m_output->textCursor();
    cursor.setPosition(m_commandStart);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.insertText(text);
    m_output->setTextCursor(cursor);
    m_output->moveCursor(QTextCursor::End);
}

void TerminalWidget::pasteAtCommand(const QString& text) {
    if (text.isEmpty()) {
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

void TerminalWidget::recallHistory(int direction) {
    if (m_history.isEmpty()) {
        return;
    }

    int next = m_historyIndex + direction;
    next = qBound(0, next, m_history.size());
    m_historyIndex = next;
    replaceCurrentCommand(next < m_history.size() ? m_history.at(next) : QString());
}

void TerminalWidget::executeCommand(const QString& command, bool echoPromptLine) {
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty()) {
        appendOutput("\n");
        insertPrompt();
        return;
    }

    static const QRegularExpression sshCommandPrefix("^ssh\\s+");
    if (sshCommandPrefix.match(trimmed).hasMatch()) {
        appendOutput("\n");
        emit sshCommandRequested(trimmed);
        insertPrompt();
        return;
    }

    if (m_process->state() != QProcess::NotRunning) {
        appendOutput("\n");
        m_process->write(trimmed.toUtf8());
        m_process->write("\n");
        m_commandStart = m_output->toPlainText().size();
        return;
    }

    const QString prepared = sanitizeCommand(trimmed);

    if (echoPromptLine) {
        appendOutput(QString("$ %1\n").arg(prepared));
    } else {
        appendOutput("\n");
    }
    m_process->setProgram("/bin/zsh");
    m_process->setArguments({"-lc", prepared});
    m_process->start();
}

void TerminalWidget::onReadyReadStdout() {
    appendOutput(normalizeTerminalOutput(QString::fromUtf8(m_process->readAllStandardOutput())));
}

void TerminalWidget::onReadyReadStderr() {
    appendOutput(normalizeTerminalOutput(QString::fromUtf8(m_process->readAllStandardError())));
}

void TerminalWidget::onProcessStateChanged(QProcess::ProcessState state) {
    switch (state) {
    case QProcess::NotRunning:
        appendStatus("[done]");
        insertPrompt();
        break;
    case QProcess::Starting:
        appendStatus("[running]");
        break;
    case QProcess::Running:
        break;
    }
}

void TerminalWidget::appendOutput(const QString& text) {
    if (!text.isEmpty()) {
        m_output->moveCursor(QTextCursor::End);
        m_output->insertPlainText(text);
        m_output->moveCursor(QTextCursor::End);
    }
}

void TerminalWidget::appendStatus(const QString& text) {
    appendOutput(text + "\n");
}

void TerminalWidget::insertPrompt() {
    const QString all = m_output->toPlainText();
    if (!all.isEmpty() && !all.endsWith('\n')) {
        appendOutput("\n");
    }
    appendOutput("$ ");
    m_commandStart = m_output->toPlainText().size();
}

void TerminalWidget::onSelectionChanged() {
    const QString selected = m_output->textCursor().selectedText();
    if (!selected.isEmpty() && QApplication::clipboard()->supportsSelection()) {
        QApplication::clipboard()->setText(selected, QClipboard::Selection);
    }
}

void TerminalWidget::showContextMenu(const QPoint& pos) {
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

QString TerminalWidget::sanitizeCommand(const QString& command) const {
    const QString trimmed = command.trimmed();

    // Prevent ssh from asking for a tty in non-terminal stdin mode.
    static const QRegularExpression sshPrefix("^ssh\\s+");
    static const QRegularExpression hasTtyOption("(^|\\s)-[A-Za-z]*[tT][A-Za-z]*(\\s|$)|(^|\\s)--tty(\\s|$)|(^|\\s)-T(\\s|$)");
    if (sshPrefix.match(trimmed).hasMatch() && !hasTtyOption.match(trimmed).hasMatch()) {
        return QString("ssh -T %1").arg(trimmed.mid(4).trimmed());
    }

    return trimmed;
}

bool TerminalWidget::isInteractiveShellCommand(const QString& command) const {
    Q_UNUSED(command);
    return false;
}

} // namespace svy::terminal
