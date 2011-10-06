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
    Selection(const BufferIterator& first, const BufferIterator& last)
        : m_first(first), m_last(last) {}

    BufferIterator begin() const;
    BufferIterator end() const;

    const BufferIterator& first() const { return m_first; }
    const BufferIterator& last()  const { return m_last; }

    void offset(int offset);

private:
    BufferIterator m_first;
    BufferIterator m_last;
};

typedef std::vector<Selection> SelectionList;

class IncrementalInserter;

class Window
{
public:
    typedef BufferString String;
    typedef std::function<Selection (const BufferIterator&)> Selector;

    enum class SelectMode
    {
        Normal,
        Append,
        LineAppend,
    };

    void erase();
    void insert(const String& string);
    void append(const String& string);

    const BufferCoord& position() const { return m_position; }
    WindowCoord cursor_position() const;

    Buffer& buffer() const { return m_buffer; }

    BufferCoord window_to_buffer(const WindowCoord& window_pos) const;
    WindowCoord buffer_to_window(const BufferCoord& buffer_pos) const;

    BufferIterator iterator_at(const WindowCoord& window_pos) const;
    WindowCoord    line_and_column_at(const BufferIterator& iterator) const;

    void move_cursor(const WindowCoord& offset);
    void move_cursor_to(const WindowCoord& new_pos);

    void clear_selections();
    void select(const Selector& selector);
    BufferString selection_content() const;

    void set_dimensions(const WindowCoord& dimensions);

    const DisplayBuffer& display_buffer() const { return m_display_buffer; }

    void update_display_buffer();

    bool undo();
    bool redo();

    SelectMode select_mode() const { return m_select_mode; }
    void set_select_mode(SelectMode select_mode) { m_select_mode = select_mode; }

    std::string status_line() const;

private:
    friend class Buffer;

    Window(Buffer& buffer);
    Window(const Window&) = delete;

    void check_invariant() const;
    void scroll_to_keep_cursor_visible_ifn();

    void erase_noundo();
    void insert_noundo(const String& string);
    void append_noundo(const String& string);

    friend class IncrementalInserter;
    IncrementalInserter* m_current_inserter;

    friend class HighlightSelections;

    SelectMode    m_select_mode;
    Buffer&       m_buffer;
    BufferCoord   m_position;
    WindowCoord   m_dimensions;
    SelectionList m_selections;
    DisplayBuffer m_display_buffer;

    typedef std::vector<std::function<void (DisplayBuffer&)>> FilterList;
    FilterList m_filters;
};

class IncrementalInserter
{
public:
    enum class Mode
    {
        Insert,
        Append,
        Change,
        OpenLineBelow,
        OpenLineAbove
    };

    IncrementalInserter(Window& window, Mode mode = Mode::Insert);
    ~IncrementalInserter();

    void insert(const Window::String& string);
    void erase();
    void move_cursor(const WindowCoord& offset);

private:
    Window&                     m_window;
};

}

#endif // window_hh_INCLUDED
