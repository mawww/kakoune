#ifndef window_hh_INCLUDED
#define window_hh_INCLUDED

#include <functional>

#include "line_and_column.hh"

#include "buffer.hh"
#include "display_buffer.hh"

namespace Kakoune
{

struct WindowCoord : LineAndColumn<WindowCoord>
{
    WindowCoord(int line = 0, int column = 0)
        : LineAndColumn(line, column) {}
};

struct Selection
{
    Selection(const BufferIterator& begin, const BufferIterator& end)
        : m_begin(begin), m_end(end) {}

    const BufferIterator& begin() const { return m_begin; }
    const BufferIterator& end() const   { return m_end; }

    void canonicalize()
    {
        if (m_end < m_begin)
            std::swap(++m_begin, ++m_end);
    }

    void offset(int offset)
    {
        m_begin += offset;
        m_end += offset;
    }

private:
    BufferIterator m_begin;
    BufferIterator m_end;
};

typedef std::vector<Selection> SelectionList;

class IncrementalInserter;

class Window
{
public:
    typedef BufferString String;
    typedef std::function<Selection (const BufferIterator&)> Selector;

    void erase();
    void insert(const String& string);
    void append(const String& string);

    const BufferCoord& position() const { return m_position; }
    const WindowCoord& cursor_position() const { return m_cursor; }

    Buffer& buffer() const { return m_buffer; }

    BufferCoord window_to_buffer(const WindowCoord& window_pos) const;
    WindowCoord buffer_to_window(const BufferCoord& buffer_pos) const;

    BufferIterator iterator_at(const WindowCoord& window_pos) const;
    WindowCoord    line_and_column_at(const BufferIterator& iterator) const;

    void move_cursor(const WindowCoord& offset);

    void empty_selections();
    void select(bool append, const Selector& selector);

    void set_dimensions(const WindowCoord& dimensions);

    const DisplayBuffer& display_buffer() const { return m_display_buffer; }

    void update_display_buffer();

    bool undo();
    bool redo();

private:
    friend class Buffer;

    Window(Buffer& buffer);
    Window(const Window&) = delete;

    void scroll_to_keep_cursor_visible_ifn();

    friend class IncrementalInserter;
    IncrementalInserter* m_current_inserter;

    Buffer&       m_buffer;
    BufferCoord   m_position;
    WindowCoord   m_cursor;
    WindowCoord   m_dimensions;
    SelectionList m_selections;
    DisplayBuffer m_display_buffer;
};

class IncrementalInserter
{
public:
    typedef std::vector<WindowCoord> CursorList;

    IncrementalInserter(Window& window, bool append = false);
    ~IncrementalInserter();

    void insert(const Window::String& string);
    void erase();
    void move_cursor(const WindowCoord& offset);

    const CursorList& cursors() const { return m_cursors; }

private:
    Window&                     m_window;
    std::vector<WindowCoord>    m_cursors;
};

}

#endif // window_hh_INCLUDED
