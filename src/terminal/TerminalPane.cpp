#include "terminal/TerminalPane.h"

#include <QVBoxLayout>

#include "terminal/PtySession.h"
#include "terminal/TerminalView.h"

namespace svy::terminal {

TerminalPane::TerminalPane(QWidget* parent)
    : QWidget(parent),
      m_view(new TerminalView(this)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view);
    setLayout(layout);

    connect(m_view, &TerminalView::titleChanged, this, &TerminalPane::titleChanged);
    connect(m_view, &TerminalView::inputProduced, this, &TerminalPane::inputProduced);
}

TerminalPane::~TerminalPane() = default;

bool TerminalPane::startProcess(const QString& program, const QStringList& arguments) {
    auto* session = new PtySession(this);
    m_session = session;
    m_view->attachSession(session);

    connect(session, &PtySession::dataReceived, m_view, &TerminalView::feed);
    connect(session, &PtySession::sessionEnded, this, [this](int exitCode) {
        m_view->feed(QByteArray("\r\n[process exited with code ") + QByteArray::number(exitCode) + "]\r\n");
        emit sessionEnded(exitCode);
    });
    connect(m_view, &TerminalView::sizeChanged, session, &PtySession::resize);

    return session->start(program, arguments, m_view->columns(), m_view->rows());
}

void TerminalPane::writeInput(const QByteArray& data) {
    if (m_session != nullptr) {
        m_session->write(data);
    }
}

void TerminalPane::runCommand(const QString& command) {
    if (command.trimmed().isEmpty()) {
        return;
    }
    writeInput(command.toUtf8() + "\r");
}

void TerminalPane::adjustFontSize(int delta) {
    m_view->adjustFontSize(delta);
}

void TerminalPane::resetFontSize() {
    m_view->resetFontSize();
}

bool TerminalPane::isRunning() const {
    return m_session != nullptr && m_session->isRunning();
}

} // namespace svy::terminal
