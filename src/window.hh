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
        : begin(begin), end(end) {}

    BufferIterator begin;
    BufferIterator end;
};

typedef std::vector<Selection> SelectionList;

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

    const SelectionList& selections() const { return m_selections; }

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

    Buffer&       m_buffer;
    BufferCoord   m_position;
    WindowCoord   m_cursor;
    WindowCoord   m_dimensions;
    SelectionList m_selections;
    DisplayBuffer m_display_buffer;
};

}

#endif // window_hh_INCLUDED
