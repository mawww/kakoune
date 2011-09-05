#ifndef window_hh_INCLUDED
#define window_hh_INCLUDED

#include <memory>
#include <functional>
#include "buffer.hh"
#include "display_buffer.hh"

namespace Kakoune
{

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

    Window(const std::shared_ptr<Buffer> buffer);
    Window(const Window&) = delete;

    void erase();
    void insert(const String& string);
    void append(const String& string);

    const LineAndColumn& position() const { return m_position; }
    const LineAndColumn& cursor_position() const { return m_cursor; }

    const std::shared_ptr<Buffer>& buffer() const { return m_buffer; }
    LineAndColumn  window_to_buffer(const LineAndColumn& window_pos) const;
    LineAndColumn  buffer_to_window(const LineAndColumn& buffer_pos) const;

    BufferIterator iterator_at(const LineAndColumn& window_pos) const;
    LineAndColumn  line_and_column_at(const BufferIterator& iterator) const;

    void move_cursor(const LineAndColumn& offset);

    const SelectionList& selections() const { return m_selections; }

    void empty_selections();
    void select(bool append, const Selector& selector);

    void set_dimensions(const LineAndColumn& dimensions);

    const DisplayBuffer& display_buffer() const { return m_display_buffer; }

    void update_display_buffer();

private:

    std::shared_ptr<Buffer> m_buffer;
    LineAndColumn           m_position;
    LineAndColumn           m_cursor;
    LineAndColumn           m_dimensions;
    SelectionList           m_selections;
    DisplayBuffer           m_display_buffer;
};

}

#endif // window_hh_INCLUDED
