#include "terminal/TerminalWidget.h"

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace svy::terminal {

TerminalWidget::TerminalWidget(QWidget* parent)
    : QWidget(parent),
      m_output(new QPlainTextEdit(this)),
      m_input(new QLineEdit(this)),
      m_process(new QProcess(this)) {
    m_output->setReadOnly(true);
    m_output->setObjectName("terminalOutput");
    m_input->setPlaceholderText("Enter command and press Return");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(m_output);
    layout->addWidget(m_input);
    setLayout(layout);

    connect(m_input, &QLineEdit::returnPressed, this, &TerminalWidget::onEnterPressed);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &TerminalWidget::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &TerminalWidget::onReadyReadStderr);
    connect(m_process, &QProcess::stateChanged, this, &TerminalWidget::onProcessStateChanged);

    appendOutput("[ready]");
}

void TerminalWidget::runCommand(const QString& command) {
    if (command.trimmed().isEmpty()) {
        return;
    }

    if (m_process->state() != QProcess::NotRunning) {
        appendOutput("[busy] previous command still running");
        return;
    }

    appendOutput(QString("$ %1").arg(command));
    m_process->setProgram("/bin/zsh");
    m_process->setArguments({"-lc", command});
    m_process->start();
}

bool TerminalWidget::isBusy() const {
    return m_process->state() != QProcess::NotRunning;
}

void TerminalWidget::onEnterPressed() {
    const QString command = m_input->text();
    m_input->clear();
    runCommand(command);
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
        appendOutput("[done]");
        break;
    case QProcess::Starting:
        appendOutput("[running]");
        break;
    case QProcess::Running:
        break;
    }
}

void TerminalWidget::appendOutput(const QString& text) {
    if (!text.isEmpty()) {
        m_output->appendPlainText(text.trimmed());
    }
}

} // namespace svy::terminal
