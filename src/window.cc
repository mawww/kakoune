#include "window.hh"

#include "assert.hh"

#include <algorithm>

namespace Kakoune
{

BufferIterator Selection::begin() const
{
    return std::min(m_first, m_last);
}

BufferIterator Selection::end() const
{
    return std::max(m_first, m_last) + 1;
}

void Selection::offset(int offset)
{
    m_first += offset;
    m_last += offset;
}

struct scoped_undo_group
{
    scoped_undo_group(Buffer& buffer)
        : m_buffer(buffer) { m_buffer.begin_undo_group(); }

    ~scoped_undo_group()   { m_buffer.end_undo_group(); }
private:
    Buffer& m_buffer;
};

Window::Window(Buffer& buffer)
    : m_buffer(buffer),
      m_position(0, 0),
      m_dimensions(0, 0),
      m_current_inserter(nullptr)
{
    m_selections.push_back(Selection(buffer.begin(), buffer.begin()));
}

void Window::check_invariant() const
{
    assert(not m_selections.empty());
}

WindowCoord Window::cursor_position() const
{
    check_invariant();
    return line_and_column_at(m_selections.back().last());
}

void Window::erase()
{
    scoped_undo_group undo_group(m_buffer);
    erase_noundo();
}

void Window::erase_noundo()
{
    check_invariant();
    for (auto& sel : m_selections)
    {
        m_buffer.erase(sel.begin(), sel.end());
        sel = Selection(sel.begin(), sel.begin());
    }
}

static WindowCoord measure_string(const Window::String& string)
{
    WindowCoord result(0, 0);
    for (size_t i = 0; i < string.length(); ++i)
    {
        if (string[i] == '\n')
        {
            ++result.line;
            result.column = 0;
        }
        else
            ++result.column;
    }
    return result;
}

void Window::insert(const String& string)
{
    scoped_undo_group undo_group(m_buffer);
    insert_noundo(string);
}

void Window::insert_noundo(const String& string)
{
    for (auto& sel : m_selections)
    {
        m_buffer.insert(sel.begin(), string);
        sel.offset(string.length());
    }
}

void Window::append(const String& string)
{
    scoped_undo_group undo_group(m_buffer);
    append_noundo(string);
}

void Window::append_noundo(const String& string)
{
    for (auto& sel : m_selections)
        m_buffer.insert(sel.end(), string);
}

bool Window::undo()
{
    return m_buffer.undo();
}

bool Window::redo()
{
    return m_buffer.redo();
}

BufferCoord Window::window_to_buffer(const WindowCoord& window_pos) const
{
    return BufferCoord(m_position.line + window_pos.line,
                       m_position.column + window_pos.column);
}

WindowCoord Window::buffer_to_window(const BufferCoord& buffer_pos) const
{
    return WindowCoord(buffer_pos.line - m_position.line,
                       buffer_pos.column - m_position.column);
}

BufferIterator Window::iterator_at(const WindowCoord& window_pos) const
{
    return m_buffer.iterator_at(window_to_buffer(window_pos));
}

WindowCoord Window::line_and_column_at(const BufferIterator& iterator) const
{
    return buffer_to_window(m_buffer.line_and_column_at(iterator));
}

void Window::empty_selections()
{
    check_invariant();
    Selection sel = Selection(m_selections.back().last(),
                              m_selections.back().last());
    m_selections.clear();
    m_selections.push_back(std::move(sel));
}

void Window::select(bool append, const Selector& selector)
{
    check_invariant();

    if (not append)
    {
        Selection sel = selector(m_selections.back().last());
        m_selections.clear();
        m_selections.push_back(std::move(sel));
    }
    else
    {
        for (auto& sel : m_selections)
        {
            sel = Selection(sel.first(), selector(sel.last()).last());
        }
    }
    scroll_to_keep_cursor_visible_ifn();
}

BufferString Window::selection_content() const
{
    check_invariant();

    return m_buffer.string(m_selections.back().begin(),
                           m_selections.back().end());
}

void Window::move_cursor(const WindowCoord& offset)
{
    move_cursor_to(cursor_position() + offset);
}

void Window::move_cursor_to(const WindowCoord& new_pos)
{
    BufferIterator target = iterator_at(new_pos);
    m_selections.clear();
    m_selections.push_back(Selection(target, target));

    scroll_to_keep_cursor_visible_ifn();
}

void Window::update_display_buffer()
{
    m_display_buffer.clear();

    SelectionList sorted_selections = m_selections;

    std::sort(sorted_selections.begin(), sorted_selections.end(),
              [](const Selection& lhs, const Selection& rhs) { return lhs.begin() < rhs.begin(); });

    BufferIterator current_position = m_buffer.iterator_at(m_position);

    for (Selection& sel : sorted_selections)
    {
        if (current_position != sel.begin())
        {
            DisplayAtom atom;
            atom.content = m_buffer.string(current_position, sel.begin());
            m_display_buffer.append(atom);
        }
        if (sel.begin() != sel.end())
        {
            DisplayAtom atom;
            atom.content = m_buffer.string(sel.begin(), sel.end());
            atom.attribute = Underline;
            m_display_buffer.append(atom);
        }
        current_position = sel.end();
    }
    if (current_position != m_buffer.end())
    {
        DisplayAtom atom;
        atom.content = m_buffer.string(current_position, m_buffer.end());
        m_display_buffer.append(atom);
    }
}

void Window::set_dimensions(const WindowCoord& dimensions)
{
    m_dimensions = dimensions;
}

void Window::scroll_to_keep_cursor_visible_ifn()
{
    check_invariant();

    WindowCoord cursor = line_and_column_at(m_selections.back().last());
    if (cursor.line < 0)
    {
        m_position.line = std::max(m_position.line + cursor.line, 0);
    }
    else if (cursor.line >= m_dimensions.line)
    {
        m_position.line += cursor.line - (m_dimensions.line - 1);
    }

    if (cursor.column < 0)
    {
        m_position.column = std::max(m_position.column + cursor.column, 0);
    }
    else if (cursor.column >= m_dimensions.column)
    {
        m_position.column += cursor.column - (m_dimensions.column - 1);
    }
}

IncrementalInserter::IncrementalInserter(Window& window, bool append)
    : m_window(window)
{
    assert(not m_window.m_current_inserter);
    m_window.m_current_inserter = this;
    m_window.check_invariant();

    for (auto& sel : m_window.m_selections)
    {
        const BufferIterator& pos = append ? sel.end() : sel.begin();
        sel = Selection(pos, pos);
    }
    m_window.m_buffer.begin_undo_group();
}

IncrementalInserter::~IncrementalInserter()
{
    assert(m_window.m_current_inserter == this);
    m_window.m_current_inserter = nullptr;

    m_window.m_buffer.end_undo_group();
}

void IncrementalInserter::insert(const Window::String& string)
{
    m_window.insert_noundo(string);
}

void IncrementalInserter::erase()
{
    move_cursor(WindowCoord(0, -1));
    m_window.erase_noundo();
}

void IncrementalInserter::move_cursor(const WindowCoord& offset)
{
    for (auto& sel : m_window.m_selections)
    {
        WindowCoord pos = m_window.line_and_column_at(sel.last());
        BufferIterator it = m_window.iterator_at(pos + offset);
        sel = Selection(it, it);
    }
}

}
