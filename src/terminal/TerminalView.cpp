#include "terminal/TerminalView.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMenu>
#include <QAbstractButton>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScopeGuard>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

#include "terminal/TerminalSession.h"

namespace svy::terminal {

namespace {

const QColor kDefaultForeground(0xE5, 0xE7, 0xEB);
const QColor kDefaultBackground(0x0B, 0x12, 0x20);

int handleDamage(VTermRect, void* user);
int handleMoveRect(VTermRect, VTermRect, void* user);
int handleMoveCursor(VTermPos pos, VTermPos, int visible, void* user);
int handleSetTermProp(VTermProp prop, VTermValue* value, void* user);
int handleBell(void* user);
int handleResize(int rows, int cols, void* user);
int handlePushLine(int cols, const VTermScreenCell* cells, void* user);
int handlePopLine(int cols, VTermScreenCell* cells, void* user);

void handleOutput(const char* bytes, size_t length, void* user);

const VTermScreenCallbacks kScreenCallbacks = {
    handleDamage,
    handleMoveRect,
    handleMoveCursor,
    handleSetTermProp,
    handleBell,
    handleResize,
    handlePushLine,
    handlePopLine,
    nullptr,
};

} // namespace

struct VTermCallbackBridge {
    static int damage(TerminalView* view) {
        view->scheduleRepaint();
        return 1;
    }

    static int moveCursor(TerminalView* view, VTermPos pos, int visible) {
        view->m_cursor = pos;
        view->m_cursorVisible = visible != 0;
        view->scheduleRepaint();
        return 1;
    }

    static int setProp(TerminalView* view, VTermProp prop, VTermValue* value) {
        switch (prop) {
        case VTERM_PROP_CURSORVISIBLE:
            view->m_cursorVisible = value->boolean != 0;
            break;
        case VTERM_PROP_ALTSCREEN:
            view->m_altScreen = value->boolean != 0;
            view->updateScrollbar();
            break;
        case VTERM_PROP_MOUSE:
            view->m_mouseTrackingEnabled = value->number != VTERM_PROP_MOUSE_NONE;
            break;
        case VTERM_PROP_TITLE:
            if (value->string.str != nullptr) {
                emit view->titleChanged(QString::fromUtf8(value->string.str, value->string.len));
            }
            break;
        default:
            break;
        }
        view->scheduleRepaint();
        return 1;
    }

    static int bell(TerminalView* view) {
        emit view->bellRang();
        return 1;
    }

    static int pushLine(TerminalView* view, int cols, const VTermScreenCell* cells) {
        QVector<TerminalView::Cell> line;
        line.reserve(cols);
        for (int i = 0; i < cols; ++i) {
            line.append(view->convertCell(cells[i]));
        }
        view->m_scrollback.append(line);
        while (view->m_scrollback.size() > view->m_scrollbackLimit) {
            view->m_scrollback.removeFirst();
        }
        view->updateScrollbar();
        return 1;
    }

    static int popLine(TerminalView* view, int cols, VTermScreenCell* cells) {
        if (view->m_scrollback.isEmpty()) {
            return 0;
        }
        const QVector<TerminalView::Cell> line = view->m_scrollback.takeLast();
        for (int i = 0; i < cols; ++i) {
            cells[i].chars[0] = i < line.size() && !line.at(i).text.isEmpty()
                                    ? line.at(i).text.at(0).unicode()
                                    : static_cast<uint32_t>(' ');
            cells[i].width = 1;
        }
        view->updateScrollbar();
        return 1;
    }

    static void output(TerminalView* view, const char* bytes, size_t length) {
        view->writeToSession(bytes, static_cast<int>(length));
    }
};

namespace {

int handleDamage(VTermRect, void* user) {
    return VTermCallbackBridge::damage(static_cast<TerminalView*>(user));
}

int handleMoveRect(VTermRect, VTermRect, void* user) {
    return VTermCallbackBridge::damage(static_cast<TerminalView*>(user));
}

int handleMoveCursor(VTermPos pos, VTermPos, int visible, void* user) {
    return VTermCallbackBridge::moveCursor(static_cast<TerminalView*>(user), pos, visible);
}

int handleSetTermProp(VTermProp prop, VTermValue* value, void* user) {
    return VTermCallbackBridge::setProp(static_cast<TerminalView*>(user), prop, value);
}

int handleBell(void* user) {
    return VTermCallbackBridge::bell(static_cast<TerminalView*>(user));
}

int handleResize(int, int, void* user) {
    return VTermCallbackBridge::damage(static_cast<TerminalView*>(user));
}

int handlePushLine(int cols, const VTermScreenCell* cells, void* user) {
    return VTermCallbackBridge::pushLine(static_cast<TerminalView*>(user), cols, cells);
}

int handlePopLine(int cols, VTermScreenCell* cells, void* user) {
    return VTermCallbackBridge::popLine(static_cast<TerminalView*>(user), cols, cells);
}

void handleOutput(const char* bytes, size_t length, void* user) {
    VTermCallbackBridge::output(static_cast<TerminalView*>(user), bytes, length);
}

} // namespace

TerminalView::TerminalView(QWidget* parent)
    : QAbstractScrollArea(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    viewport()->setAutoFillBackground(false);
    viewport()->setCursor(Qt::IBeamCursor);

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(12);
    m_defaultFontSize = font.pointSize();
    applyFont(font);

    m_vterm = vterm_new(m_rows, m_columns);
    vterm_set_utf8(m_vterm, 1);
    vterm_output_set_callback(m_vterm, handleOutput, this);

    m_screen = vterm_obtain_screen(m_vterm);
    m_state = vterm_obtain_state(m_vterm);

    VTermColor defaultFg;
    VTermColor defaultBg;
    vterm_color_rgb(&defaultFg, kDefaultForeground.red(), kDefaultForeground.green(), kDefaultForeground.blue());
    vterm_color_rgb(&defaultBg, kDefaultBackground.red(), kDefaultBackground.green(), kDefaultBackground.blue());
    vterm_state_set_default_colors(m_state, &defaultFg, &defaultBg);

    vterm_screen_set_callbacks(m_screen, &kScreenCallbacks, this);
    vterm_screen_enable_altscreen(m_screen, 1);
    vterm_screen_reset(m_screen, 1);
}

TerminalView::~TerminalView() {
    if (m_vterm != nullptr) {
        vterm_free(m_vterm);
        m_vterm = nullptr;
    }
}

void TerminalView::attachSession(TerminalSession* session) {
    m_session = session;
}

void TerminalView::feed(const QByteArray& data) {
    if (m_vterm == nullptr || data.isEmpty()) {
        return;
    }
    vterm_input_write(m_vterm, data.constData(), static_cast<size_t>(data.size()));
    vterm_screen_flush_damage(m_screen);
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    scheduleRepaint();
}

void TerminalView::applyFont(const QFont& font) {
    QFont fixed = font;
    fixed.setFixedPitch(true);
    fixed.setStyleHint(QFont::Monospace);
    setFont(fixed);

    const QFontMetricsF metrics(fixed);
    m_cellWidth = qMax<qreal>(metrics.horizontalAdvance(QChar('W')), 1.0);
    m_cellHeight = qMax<qreal>(metrics.height(), 1.0);
    m_baseline = static_cast<int>(metrics.ascent());
}

void TerminalView::adjustFontSize(int delta) {
    QFont current = font();
    const int size = qBound(6, (current.pointSize() > 0 ? current.pointSize() : m_defaultFontSize) + delta, 48);
    current.setPointSize(size);
    applyFont(current);
    recomputeGrid();
    scheduleRepaint();
}

void TerminalView::resetFontSize() {
    QFont current = font();
    current.setPointSize(m_defaultFontSize);
    applyFont(current);
    recomputeGrid();
    scheduleRepaint();
}

void TerminalView::recomputeGrid() {
    const int columns = qMax(1, static_cast<int>(viewport()->width() / m_cellWidth));
    const int rows = qMax(1, static_cast<int>(viewport()->height() / m_cellHeight));
    if (columns == m_columns && rows == m_rows) {
        return;
    }

    m_columns = columns;
    m_rows = rows;
    vterm_set_size(m_vterm, m_rows, m_columns);
    vterm_screen_flush_damage(m_screen);
    updateScrollbar();
    emit sizeChanged(m_columns, m_rows);
}

void TerminalView::scheduleRepaint() {
    if (m_repaintScheduled) {
        return;
    }
    m_repaintScheduled = true;
    QTimer::singleShot(16, this, [this]() {
        m_repaintScheduled = false;
        viewport()->update();
    });
}

void TerminalView::updateScrollbar() {
    const int scrollbackLines = m_altScreen ? 0 : m_scrollback.size();
    verticalScrollBar()->setRange(0, scrollbackLines);
    verticalScrollBar()->setPageStep(m_rows);
    verticalScrollBar()->setSingleStep(1);
}

int TerminalView::topVisibleLine() const {
    return m_altScreen ? 0 : verticalScrollBar()->value();
}

int TerminalView::totalLines() const {
    return (m_altScreen ? 0 : m_scrollback.size()) + m_rows;
}

TerminalView::Cell TerminalView::convertCell(const VTermScreenCell& source) const {
    Cell cell;
    if (source.chars[0] != 0 && source.chars[0] != 0xFFFFFFFF) {
        char32_t chars[VTERM_MAX_CHARS_PER_CELL + 1];
        int count = 0;
        while (count < VTERM_MAX_CHARS_PER_CELL && source.chars[count] != 0) {
            chars[count] = static_cast<char32_t>(source.chars[count]);
            ++count;
        }
        cell.text = QString::fromUcs4(chars, count);
    }

    cell.bold = source.attrs.bold != 0;
    cell.underline = source.attrs.underline != VTERM_UNDERLINE_OFF;
    cell.italic = source.attrs.italic != 0;
    cell.reverse = source.attrs.reverse != 0;
    cell.wide = source.width > 1;
    cell.foreground = toQColor(source.fg, true);
    cell.background = toQColor(source.bg, false);
    return cell;
}

QColor TerminalView::toQColor(const VTermColor& color, bool foreground) const {
    if (VTERM_COLOR_IS_DEFAULT_FG(&color)) {
        return kDefaultForeground;
    }
    if (VTERM_COLOR_IS_DEFAULT_BG(&color)) {
        return kDefaultBackground;
    }

    VTermColor resolved = color;
    vterm_screen_convert_color_to_rgb(m_screen, &resolved);
    if (VTERM_COLOR_IS_RGB(&resolved)) {
        return QColor(resolved.rgb.red, resolved.rgb.green, resolved.rgb.blue);
    }
    return foreground ? kDefaultForeground : kDefaultBackground;
}

TerminalView::Cell TerminalView::cellAt(int row, int column) const {
    const int scrollbackLines = m_altScreen ? 0 : m_scrollback.size();
    if (row < scrollbackLines) {
        const QVector<Cell>& line = m_scrollback.at(row);
        if (column < line.size()) {
            return line.at(column);
        }
        Cell empty;
        empty.foreground = kDefaultForeground;
        empty.background = kDefaultBackground;
        return empty;
    }

    VTermPos pos;
    pos.row = row - scrollbackLines;
    pos.col = column;

    VTermScreenCell source;
    if (pos.row < 0 || pos.row >= m_rows || vterm_screen_get_cell(m_screen, pos, &source) == 0) {
        Cell empty;
        empty.foreground = kDefaultForeground;
        empty.background = kDefaultBackground;
        return empty;
    }
    return convertCell(source);
}

void TerminalView::paintEvent(QPaintEvent*) {
    QPainter painter(viewport());
    painter.setFont(font());
    painter.fillRect(viewport()->rect(), kDefaultBackground);

    const int firstLine = topVisibleLine();
    const int scrollbackLines = m_altScreen ? 0 : m_scrollback.size();
    const int visibleRows = qMin(m_rows + (m_altScreen ? 0 : scrollbackLines) - firstLine,
                                 static_cast<int>(viewport()->height() / m_cellHeight) + 1);

    QPoint selectionStart = m_selectionAnchor;
    QPoint selectionEnd = m_selectionHead;
    if (selectionStart.y() > selectionEnd.y() ||
        (selectionStart.y() == selectionEnd.y() && selectionStart.x() > selectionEnd.x())) {
        std::swap(selectionStart, selectionEnd);
    }
    const bool hasSelection = selectionStart.y() >= 0 && selectionEnd.y() >= 0;

    for (int screenRow = 0; screenRow < visibleRows; ++screenRow) {
        const int documentRow = firstLine + screenRow;
        const qreal y = screenRow * m_cellHeight;

        for (int column = 0; column < m_columns; ++column) {
            const Cell cell = cellAt(documentRow, column);
            const qreal x = column * m_cellWidth;
            const QRectF cellRect(x, y, m_cellWidth * (cell.wide ? 2 : 1), m_cellHeight);

            QColor foreground = cell.foreground;
            QColor background = cell.background;
            if (cell.reverse) {
                std::swap(foreground, background);
            }

            bool selected = false;
            if (hasSelection) {
                const bool afterStart = documentRow > selectionStart.y() ||
                                        (documentRow == selectionStart.y() && column >= selectionStart.x());
                const bool beforeEnd = documentRow < selectionEnd.y() ||
                                       (documentRow == selectionEnd.y() && column <= selectionEnd.x());
                selected = afterStart && beforeEnd;
            }
            if (selected) {
                std::swap(foreground, background);
            }

            if (background != kDefaultBackground || selected) {
                painter.fillRect(cellRect, background);
            }

            if (!cell.text.isEmpty() && cell.text != QStringLiteral(" ")) {
                QFont cellFont = font();
                cellFont.setBold(cell.bold);
                cellFont.setItalic(cell.italic);
                cellFont.setUnderline(cell.underline);
                painter.setFont(cellFont);
                painter.setPen(foreground);
                painter.drawText(QPointF(x, y + m_baseline), cell.text);
            }
        }
    }

    const int cursorDocumentRow = scrollbackLines + m_cursor.row;
    if (m_cursorVisible && cursorDocumentRow >= firstLine && cursorDocumentRow < firstLine + visibleRows) {
        const QRectF cursorRect((m_cursor.col) * m_cellWidth,
                                (cursorDocumentRow - firstLine) * m_cellHeight,
                                m_cellWidth,
                                m_cellHeight);
        if (m_hasFocus) {
            painter.fillRect(cursorRect, kDefaultForeground);
            const Cell cell = cellAt(cursorDocumentRow, m_cursor.col);
            if (!cell.text.isEmpty()) {
                painter.setPen(kDefaultBackground);
                painter.setFont(font());
                painter.drawText(QPointF(cursorRect.left(), cursorRect.top() + m_baseline), cell.text);
            }
        } else {
            painter.setPen(kDefaultForeground);
            painter.drawRect(cursorRect.adjusted(0, 0, -1, -1));
        }
    }
}

void TerminalView::resizeEvent(QResizeEvent* event) {
    QAbstractScrollArea::resizeEvent(event);
    recomputeGrid();
}

void TerminalView::focusInEvent(QFocusEvent* event) {
    QAbstractScrollArea::focusInEvent(event);
    m_hasFocus = true;
    scheduleRepaint();
}

void TerminalView::focusOutEvent(QFocusEvent* event) {
    QAbstractScrollArea::focusOutEvent(event);
    m_hasFocus = false;
    scheduleRepaint();
}

bool TerminalView::focusNextPrevChild(bool) {
    return false;
}

void TerminalView::writeToSession(const char* bytes, int length) {
    if (m_session == nullptr || length <= 0) {
        return;
    }
    const QByteArray payload(bytes, length);
    m_session->write(payload);
    if (m_emitInput) {
        emit inputProduced(payload);
    }
}

bool TerminalView::handleClipboardShortcut(QKeyEvent* event) {
#if defined(Q_OS_MACOS)
    const bool copyRequested = event->modifiers().testFlag(Qt::ControlModifier) && event->key() == Qt::Key_C;
    const bool pasteRequested = event->modifiers().testFlag(Qt::ControlModifier) && event->key() == Qt::Key_V;
#else
    const bool copyRequested = event->modifiers().testFlag(Qt::ControlModifier) &&
                               event->modifiers().testFlag(Qt::ShiftModifier) && event->key() == Qt::Key_C;
    const bool pasteRequested = event->modifiers().testFlag(Qt::ControlModifier) &&
                                event->modifiers().testFlag(Qt::ShiftModifier) && event->key() == Qt::Key_V;
#endif

    if (copyRequested && !selectedText().isEmpty()) {
        copySelection();
        return true;
    }
    if (pasteRequested) {
        pasteFromClipboard();
        return true;
    }
    return false;
}

void TerminalView::keyPressEvent(QKeyEvent* event) {
    if (handleClipboardShortcut(event)) {
        return;
    }

    if (event->key() == Qt::Key_PageUp && event->modifiers().testFlag(Qt::ShiftModifier)) {
        verticalScrollBar()->setValue(verticalScrollBar()->value() - m_rows);
        return;
    }
    if (event->key() == Qt::Key_PageDown && event->modifiers().testFlag(Qt::ShiftModifier)) {
        verticalScrollBar()->setValue(verticalScrollBar()->value() + m_rows);
        return;
    }

    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    sendKeyEvent(event);
}

void TerminalView::sendKeyEvent(QKeyEvent* event) {
    m_emitInput = true;
    const auto resetGuard = qScopeGuard([this]() { m_emitInput = false; });

    VTermModifier modifiers = VTERM_MOD_NONE;
#if defined(Q_OS_MACOS)
    const bool controlPressed = event->modifiers().testFlag(Qt::MetaModifier);
    const bool altPressed = event->modifiers().testFlag(Qt::AltModifier);
#else
    const bool controlPressed = event->modifiers().testFlag(Qt::ControlModifier);
    const bool altPressed = event->modifiers().testFlag(Qt::AltModifier);
#endif
    if (controlPressed) {
        modifiers = static_cast<VTermModifier>(modifiers | VTERM_MOD_CTRL);
    }
    if (altPressed) {
        modifiers = static_cast<VTermModifier>(modifiers | VTERM_MOD_ALT);
    }
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        modifiers = static_cast<VTermModifier>(modifiers | VTERM_MOD_SHIFT);
    }

    VTermKey key = VTERM_KEY_NONE;
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        key = VTERM_KEY_ENTER;
        break;
    case Qt::Key_Backspace:
        key = VTERM_KEY_BACKSPACE;
        break;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        key = VTERM_KEY_TAB;
        break;
    case Qt::Key_Escape:
        key = VTERM_KEY_ESCAPE;
        break;
    case Qt::Key_Up:
        key = VTERM_KEY_UP;
        break;
    case Qt::Key_Down:
        key = VTERM_KEY_DOWN;
        break;
    case Qt::Key_Left:
        key = VTERM_KEY_LEFT;
        break;
    case Qt::Key_Right:
        key = VTERM_KEY_RIGHT;
        break;
    case Qt::Key_Insert:
        key = VTERM_KEY_INS;
        break;
    case Qt::Key_Delete:
        key = VTERM_KEY_DEL;
        break;
    case Qt::Key_Home:
        key = VTERM_KEY_HOME;
        break;
    case Qt::Key_End:
        key = VTERM_KEY_END;
        break;
    case Qt::Key_PageUp:
        key = VTERM_KEY_PAGEUP;
        break;
    case Qt::Key_PageDown:
        key = VTERM_KEY_PAGEDOWN;
        break;
    default:
        break;
    }

    if (key == VTERM_KEY_NONE && event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F24) {
        key = static_cast<VTermKey>(VTERM_KEY_FUNCTION_0 + (event->key() - Qt::Key_F1 + 1));
    }

    if (key != VTERM_KEY_NONE) {
        vterm_keyboard_key(m_vterm, key, modifiers);
        return;
    }

    if (controlPressed && event->key() >= Qt::Key_A && event->key() <= Qt::Key_Z) {
        const uint32_t codepoint = static_cast<uint32_t>('a' + (event->key() - Qt::Key_A));
        vterm_keyboard_unichar(m_vterm, codepoint, modifiers);
        return;
    }

    const QString text = event->text();
    if (text.isEmpty()) {
        return;
    }

    for (const uint codepoint : text.toUcs4()) {
        if (codepoint == 0) {
            continue;
        }
        vterm_keyboard_unichar(m_vterm, codepoint, modifiers);
    }
}

void TerminalView::inputMethodEvent(QInputMethodEvent* event) {
    const QString text = event->commitString();
    for (const uint codepoint : text.toUcs4()) {
        vterm_keyboard_unichar(m_vterm, codepoint, VTERM_MOD_NONE);
    }
    event->accept();
}

QPoint TerminalView::documentPositionAt(const QPoint& viewportPoint) const {
    const int column = qBound(0, static_cast<int>(viewportPoint.x() / m_cellWidth), m_columns - 1);
    const int row = topVisibleLine() + static_cast<int>(viewportPoint.y() / m_cellHeight);
    return {column, qBound(0, row, qMax(0, totalLines() - 1))};
}

void TerminalView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_selecting = true;
        m_selectionAnchor = documentPositionAt(event->pos());
        m_selectionHead = m_selectionAnchor;
        scheduleRepaint();
        return;
    }

    if (event->button() == Qt::MiddleButton && QApplication::clipboard()->supportsSelection()) {
        const QString text = QApplication::clipboard()->text(QClipboard::Selection);
        if (!text.isEmpty()) {
            m_session->write(text.toUtf8());
        }
        return;
    }

    QAbstractScrollArea::mousePressEvent(event);
}

void TerminalView::mouseMoveEvent(QMouseEvent* event) {
    if (!m_selecting) {
        QAbstractScrollArea::mouseMoveEvent(event);
        return;
    }
    m_selectionHead = documentPositionAt(event->pos());
    scheduleRepaint();
}

void TerminalView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        const QString text = selectedText();
        if (!text.isEmpty()) {
            if (QApplication::clipboard()->supportsSelection()) {
                QApplication::clipboard()->setText(text, QClipboard::Selection);
            }
            if (m_copyOnSelect) {
                QApplication::clipboard()->setText(text);
            }
        }
        return;
    }
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void TerminalView::wheelEvent(QWheelEvent* event) {
    if (m_altScreen) {
        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

void TerminalView::contextMenuEvent(QContextMenuEvent* event) {
    // Default: right-click pastes directly (MobaXterm style).
    // If a URL-like word is under the cursor, offer Open/Copy Link instead.
    if (m_rightClickPaste) {
        // Check if there's a URL under the cursor position
        const QPoint docPos = documentPositionAt(event->pos());
        const Cell cell = cellAt(docPos.y(), docPos.x());
        const bool looksLikeUrl = cell.text.startsWith("http") || cell.text.startsWith("ftp");
        if (looksLikeUrl) {
            // Build the full word around the cursor to get the URL
            QString word;
            int col = docPos.x();
            while (col > 0) {
                const QString ch = cellAt(docPos.y(), col - 1).text;
                if (ch.isEmpty() || ch == " ") break;
                --col;
            }
            while (col < m_columns) {
                const QString ch = cellAt(docPos.y(), col).text;
                if (ch.isEmpty() || ch == " ") break;
                word += ch;
                ++col;
            }
            if (word.startsWith("http") || word.startsWith("ftp")) {
                QMenu menu(this);
                QAction* openLink = menu.addAction("Open Link");
                QAction* copyLink = menu.addAction("Copy Link");
                QAction* chosen = menu.exec(event->globalPos());
                if (chosen == openLink) {
                    QApplication::clipboard()->setText(word);
                } else if (chosen == copyLink) {
                    QApplication::clipboard()->setText(word);
                }
                return;
            }
        }
        pasteFromClipboard();
        return;
    }

    // Context menu mode
    QMenu menu(this);
    QAction* copyAction = menu.addAction("Copy");
    QAction* pasteAction = menu.addAction("Paste");
    copyAction->setEnabled(!selectedText().isEmpty());
    pasteAction->setEnabled(!QApplication::clipboard()->text().isEmpty());

    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == copyAction) {
        copySelection();
    } else if (chosen == pasteAction) {
        pasteFromClipboard();
    }
}

QString TerminalView::selectedText() const {
    QPoint start = m_selectionAnchor;
    QPoint end = m_selectionHead;
    if (start.y() < 0 || end.y() < 0) {
        return {};
    }
    if (start.y() > end.y() || (start.y() == end.y() && start.x() > end.x())) {
        std::swap(start, end);
    }

    QString text;
    for (int row = start.y(); row <= end.y(); ++row) {
        const int firstColumn = row == start.y() ? start.x() : 0;
        const int lastColumn = row == end.y() ? end.x() : m_columns - 1;

        QString line;
        for (int column = firstColumn; column <= lastColumn; ++column) {
            const Cell cell = cellAt(row, column);
            line += cell.text.isEmpty() ? QStringLiteral(" ") : cell.text;
        }
        while (line.endsWith(' ')) {
            line.chop(1);
        }
        text += line;
        if (row != end.y()) {
            text += '\n';
        }
    }
    return text;
}

void TerminalView::copySelection() {
    const QString text = selectedText();
    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
    }
}

void TerminalView::pasteFromClipboard() {
    const QString text = QApplication::clipboard()->text();
    if (text.isEmpty() || m_session == nullptr) {
        return;
    }

    // Warn before pasting multiple lines (protects against accidental execution)
    if (m_warnMultiLine && text.contains('\n') && text.trimmed().contains('\n')) {
        QMessageBox box(this);
        box.setWindowTitle("Paste Multiple Lines?");
        box.setText("The clipboard contains multiple lines.");
        box.setInformativeText("Paste all lines into the terminal? No command will be executed automatically.");
        box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Ok);
        box.button(QMessageBox::Ok)->setText("Paste");
        if (box.exec() != QMessageBox::Ok) {
            return;
        }
    }

    QByteArray payload = text.toUtf8();
    payload.replace("\r\n", "\r");
    payload.replace('\n', '\r');

    vterm_keyboard_start_paste(m_vterm);
    m_session->write(payload);
    vterm_keyboard_end_paste(m_vterm);
}

} // namespace svy::terminal
