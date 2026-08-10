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

    if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
        const QString text = m_output->toPlainText();
        const QString command = text.mid(m_commandStart);
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

    if (isInteractiveShellCommand(trimmed)) {
        appendStatus("[info] interactive shell commands are not supported in command mode.");
        appendStatus("[hint] use shell commands directly, or open an SSH session tab for remote shell work.");
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
    if (!selected.isEmpty()) {
        QApplication::clipboard()->setText(selected);
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
        QTextCursor cursor = m_output->textCursor();
        if (cursor.position() < m_commandStart) {
            cursor.movePosition(QTextCursor::End);
            m_output->setTextCursor(cursor);
        }
        m_output->insertPlainText(QApplication::clipboard()->text());
        m_output->moveCursor(QTextCursor::End);
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
    static const QRegularExpression interactiveRe("^(bash|zsh|sh|fish)\\b");
    return interactiveRe.match(command.trimmed()).hasMatch();
}

} // namespace svy::terminal
