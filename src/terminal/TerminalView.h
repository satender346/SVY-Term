#pragma once

#include <QAbstractScrollArea>
#include <QColor>
#include <QFont>
#include <QPoint>
#include <QString>
#include <QVector>

#include <vterm.h>

namespace svy::terminal {

class TerminalSession;

class TerminalView : public QAbstractScrollArea {
    Q_OBJECT

public:
    explicit TerminalView(QWidget* parent = nullptr);
    ~TerminalView() override;

    void attachSession(TerminalSession* session);
    void feed(const QByteArray& data);
    void adjustFontSize(int delta);
    void resetFontSize();
    QString selectedText() const;
    void copySelection();
    void pasteFromClipboard();

    int columns() const { return m_columns; }
    int rows() const { return m_rows; }

signals:
    void sizeChanged(int columns, int rows);
    void bellRang();
    void titleChanged(const QString& title);
    void inputProduced(const QByteArray& data);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    bool focusNextPrevChild(bool next) override;
    void inputMethodEvent(QInputMethodEvent* event) override;

private:
    struct Cell {
        QString text;
        QColor foreground;
        QColor background;
        bool bold = false;
        bool underline = false;
        bool italic = false;
        bool reverse = false;
        bool wide = false;
    };

    friend struct VTermCallbackBridge;

    void applyFont(const QFont& font);
    void recomputeGrid();
    void scheduleRepaint();
    void updateScrollbar();
    Cell cellAt(int row, int column) const;
    Cell convertCell(const VTermScreenCell& source) const;
    QColor toQColor(const VTermColor& color, bool foreground) const;
    int topVisibleLine() const;
    int totalLines() const;
    QPoint documentPositionAt(const QPoint& viewportPoint) const;
    void writeToSession(const char* bytes, int length);
    bool handleClipboardShortcut(QKeyEvent* event);
    void sendKeyEvent(QKeyEvent* event);

    VTerm* m_vterm = nullptr;
    VTermScreen* m_screen = nullptr;
    VTermState* m_state = nullptr;
    TerminalSession* m_session = nullptr;

    QVector<QVector<Cell>> m_scrollback;
    int m_scrollbackLimit = 50000;

    int m_columns = 80;
    int m_rows = 24;
    qreal m_cellWidth = 8.0;
    qreal m_cellHeight = 16.0;
    int m_baseline = 12;
    int m_defaultFontSize = 12;

    VTermPos m_cursor {0, 0};
    bool m_cursorVisible = true;
    bool m_altScreen = false;
    bool m_mouseTrackingEnabled = false;
    bool m_hasFocus = false;

    bool m_selecting = false;
    QPoint m_selectionAnchor {-1, -1};
    QPoint m_selectionHead {-1, -1};

    bool m_emitInput = false;
    bool m_repaintScheduled = false;
};

} // namespace svy::terminal
