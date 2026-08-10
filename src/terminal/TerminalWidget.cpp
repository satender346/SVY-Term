#include "terminal/TerminalWidget.h"

#include <QEvent>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QVBoxLayout>

namespace svy::terminal {

TerminalWidget::TerminalWidget(QWidget* parent)
    : QWidget(parent),
      m_output(new QPlainTextEdit(this)),
      m_process(new QProcess(this)) {
    m_output->setReadOnly(false);
    m_output->setObjectName("terminalOutput");
    m_output->installEventFilter(this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(m_output);
    setLayout(layout);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &TerminalWidget::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &TerminalWidget::onReadyReadStderr);
    connect(m_process, &QProcess::stateChanged, this, &TerminalWidget::onProcessStateChanged);

    appendStatus("[ready]");
    insertPrompt();
}

void TerminalWidget::runCommand(const QString& command) {
    executeCommand(command, true);
}

bool TerminalWidget::isBusy() const {
    return m_process->state() != QProcess::NotRunning;
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
        appendOutput("\n");
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
        insertPrompt();
        return;
    }

    if (m_process->state() != QProcess::NotRunning) {
        appendStatus("[busy] previous command still running");
        insertPrompt();
        return;
    }

    if (echoPromptLine) {
        appendOutput(QString("$ %1\n").arg(trimmed));
    }
    m_process->setProgram("/bin/zsh");
    m_process->setArguments({"-lc", trimmed});
    m_process->start();
}

void TerminalWidget::onReadyReadStdout() {
    appendOutput(QString::fromUtf8(m_process->readAllStandardOutput()));
}

void TerminalWidget::onReadyReadStderr() {
    appendOutput(QString::fromUtf8(m_process->readAllStandardError()));
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

} // namespace svy::terminal
