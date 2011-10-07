#include "window.hh"

#include "assert.hh"
#include "filters.hh"

#include <algorithm>
#include <sstream>

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

class HighlightSelections
{
public:
    HighlightSelections(Window& window)
        : m_window(window)
    {
    }

    void operator()(DisplayBuffer& display_buffer)
    {
        SelectionList sorted_selections = m_window.m_selections;

        std::sort(sorted_selections.begin(), sorted_selections.end(),
                  [](const Selection& lhs, const Selection& rhs) { return lhs.begin() < rhs.begin(); });

        auto atom_it = display_buffer.begin();
        auto sel_it = sorted_selections.begin();

        while (atom_it != display_buffer.end()
               and sel_it != sorted_selections.end())
        {
            Selection& sel = *sel_it;
            DisplayAtom& atom = *atom_it;

            // [###------]
            if (atom.begin >= sel.begin() and atom.begin < sel.end() and atom.end > sel.end())
            {
                size_t length = sel.end() - atom.begin;
                atom_it = display_buffer.split(atom_it, length);
                atom_it->attribute |= Attributes::Underline;
                ++atom_it;
                ++sel_it;
            }
            // [---###---]
            else if (atom.begin < sel.begin() and atom.end > sel.end())
            {
                size_t prefix_length = sel.begin() - atom.begin;
                atom_it = display_buffer.split(atom_it, prefix_length);
                size_t sel_length = sel.end() - sel.begin();
                atom_it = display_buffer.split(atom_it + 1, sel_length);
                atom_it->attribute |= Attributes::Underline;
                ++atom_it;
                ++sel_it;
            }
            // [------###]
            else if (atom.begin < sel.begin() and atom.end > sel.begin())
            {
                size_t length = sel.begin() - atom.begin;
                atom_it = display_buffer.split(atom_it, length) + 1;
                atom_it->attribute |= Attributes::Underline;
                ++atom_it;
            }
            // [#########]
            else if (atom.begin >= sel.begin() and atom.end <= sel.end())
            {
                atom_it->attribute |= Attributes::Underline;
                ++atom_it;
            }
            // [---------]
            else if (atom.begin >= sel.end())
                ++sel_it;
            // [---------]
            else if (atom.end <= sel.begin())
                ++atom_it;
            else
                assert(false);
        }
    }

private:
    const Window& m_window;
};

Window::Window(Buffer& buffer)
    : m_buffer(buffer),
      m_position(0, 0),
      m_dimensions(0, 0),
      m_current_inserter(nullptr)
{
    m_selections.push_back(Selection(buffer.begin(), buffer.begin()));
    m_filters.push_back(colorize_cplusplus);
    m_filters.push_back(HighlightSelections(*this));
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

void Window::clear_selections()
{
    check_invariant();
    Selection sel = Selection(m_selections.back().last(),
                              m_selections.back().last());
    m_selections.clear();
    m_selections.push_back(std::move(sel));
}

void Window::select(const Selector& selector, bool append)
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

void Window::move_cursor(const WindowCoord& offset, bool append)
{
    if (not append)
        move_cursor_to(cursor_position() + offset);
    else
    {
        for (auto& sel : m_selections)
        {
            WindowCoord pos = line_and_column_at(sel.last());
            sel = Selection(sel.first(), iterator_at(pos + offset));
        }
        scroll_to_keep_cursor_visible_ifn();
    }
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

    BufferIterator begin = m_buffer.iterator_at(m_position);
    BufferIterator end = m_buffer.iterator_at(m_position +
                                              BufferCoord(m_dimensions.line, m_dimensions.column+1));
    m_display_buffer.append(DisplayAtom(begin, end, m_buffer.string(begin, end)));

    for (auto& filter : m_filters)
    {
        filter(m_display_buffer);
        m_display_buffer.check_invariant();
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

std::string Window::status_line() const
{
    BufferCoord cursor = window_to_buffer(cursor_position());
    std::ostringstream oss;
    oss << m_buffer.name();
    if (m_buffer.is_modified())
        oss << " [+]";
    oss << " -- " << cursor.line << "," << cursor.column
        << " -- " << m_selections.size() << " sel -- ";
    if (m_current_inserter)
        oss << "[Insert]";
    return oss.str();
}

IncrementalInserter::IncrementalInserter(Window& window, Mode mode)
    : m_window(window)
{
    assert(not m_window.m_current_inserter);
    m_window.m_current_inserter = this;
    m_window.check_invariant();

    m_window.m_buffer.begin_undo_group();

    if (mode == Mode::Change)
        window.erase_noundo();

    for (auto& sel : m_window.m_selections)
    {
        BufferIterator pos;
        switch (mode)
        {
        case Mode::Insert: pos = sel.begin(); break;
        case Mode::Append: pos = sel.end(); break;
        case Mode::Change: pos = sel.begin(); break;

        case Mode::OpenLineBelow:
            pos = sel.end();
            while (not pos.is_end() and *pos != '\n')
                ++pos;
            ++pos;
            window.m_buffer.insert(pos, "\n");
            break;

        case Mode::OpenLineAbove:
            pos = sel.begin();
            while (not pos.is_begin() and *pos != '\n')
                --pos;
            window.m_buffer.insert(pos, "\n");
            ++pos;
            break;
        }
        sel = Selection(pos, pos);
    }
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
