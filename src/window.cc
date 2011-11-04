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

void Selection::merge_with(const Selection& selection)
{
    if (m_first <= m_last)
        m_first = std::min(m_first, selection.m_first);
    else
        m_first = std::max(m_first, selection.m_first);
    m_last = selection.m_last;
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
            if (atom.begin() >= sel.begin() and atom.begin() < sel.end() and atom.end() > sel.end())
            {
                atom_it = display_buffer.split(atom_it, sel.end());
                atom_it->attribute() |= Attributes::Underline;
                ++atom_it;
                ++sel_it;
            }
            // [---###---]
            else if (atom.begin() < sel.begin() and atom.end() > sel.end())
            {
                atom_it = display_buffer.split(atom_it, sel.begin());
                atom_it = display_buffer.split(++atom_it, sel.end());
                atom_it->attribute() |= Attributes::Underline;
                ++atom_it;
                ++sel_it;
            }
            // [------###]
            else if (atom.begin() < sel.begin() and atom.end() > sel.begin())
            {
                atom_it = ++display_buffer.split(atom_it, sel.begin());
                atom_it->attribute() |= Attributes::Underline;
                ++atom_it;
            }
            // [#########]
            else if (atom.begin() >= sel.begin() and atom.end() <= sel.end())
            {
                atom_it->attribute() |= Attributes::Underline;
                ++atom_it;
            }
            // [---------]
            else if (atom.begin() >= sel.end())
                ++sel_it;
            // [---------]
            else if (atom.end() <= sel.begin())
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
    m_filters.push_back(expand_tabulations);
    m_filters.push_back(HighlightSelections(*this));
    m_filters.push_back(show_line_numbers);
}

void Window::check_invariant() const
{
    assert(not m_selections.empty());
}

DisplayCoord Window::cursor_position() const
{
    check_invariant();
    return line_and_column_at(cursor_iterator());
}

BufferIterator Window::cursor_iterator() const
{
    check_invariant();
    return m_selections.back().last();
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
        m_buffer.erase(sel.begin(), sel.end());
    scroll_to_keep_cursor_visible_ifn();
}

template<typename Iterator>
static DisplayCoord measure_string(Iterator begin, Iterator end)
{
    DisplayCoord result(0, 0);
    while (begin != end)
    {
        if (*begin == '\n')
        {
            ++result.line;
            result.column = 0;
        }
        else
            ++result.column;
        ++begin;
    }
    return result;
}

static DisplayCoord measure_string(const Window::String& string)
{
    return measure_string(string.begin(), string.end());
}

void Window::insert(const String& string)
{
    scoped_undo_group undo_group(m_buffer);
    insert_noundo(string);
}

void Window::insert_noundo(const String& string)
{
    for (auto& sel : m_selections)
        m_buffer.insert(sel.begin(), string);
    scroll_to_keep_cursor_visible_ifn();
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
    scroll_to_keep_cursor_visible_ifn();
}

bool Window::undo()
{
    return m_buffer.undo();
}

bool Window::redo()
{
    return m_buffer.redo();
}

BufferIterator Window::iterator_at(const DisplayCoord& window_pos) const
{
    if (m_display_buffer.begin() == m_display_buffer.end())
        return m_buffer.begin();

    if (DisplayCoord(0,0) <= window_pos)
    {
        for (auto atom_it = m_display_buffer.begin();
             atom_it != m_display_buffer.end(); ++atom_it)
        {
            if (window_pos < atom_it->coord())
            {
                return (--atom_it)->iterator_at(window_pos);
            }
        }
    }

    return m_buffer.iterator_at(m_position + BufferCoord(window_pos));
}

DisplayCoord Window::line_and_column_at(const BufferIterator& iterator) const
{
    if (m_display_buffer.begin() == m_display_buffer.end())
        return DisplayCoord(0, 0);

    if (iterator >= m_display_buffer.front().begin() and
        iterator <  m_display_buffer.back().end())
    {
        for (auto& atom : m_display_buffer)
        {
            if (atom.end() > iterator)
            {
                assert(atom.begin() <= iterator);
                return atom.line_and_column_at(iterator);
            }
        }
    }
    BufferCoord coord = m_buffer.line_and_column_at(iterator);
    return DisplayCoord(coord.line - m_position.line,
                        coord.column - m_position.column);
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
            sel.merge_with(selector(sel.last()));
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

void Window::move_cursor(const DisplayCoord& offset, bool append)
{
    if (not append)
    {
        BufferCoord pos = m_buffer.line_and_column_at(cursor_iterator());
        move_cursor_to(m_buffer.iterator_at(pos + BufferCoord(offset)));
    }
    else
    {
        for (auto& sel : m_selections)
        {
            BufferCoord pos = m_buffer.line_and_column_at(sel.last());
            sel = Selection(sel.first(), m_buffer.iterator_at(pos + BufferCoord(offset)));
        }
        scroll_to_keep_cursor_visible_ifn();
    }
}

void Window::move_cursor_to(const BufferIterator& iterator)
{
    m_selections.clear();
    m_selections.push_back(Selection(iterator, iterator));

    scroll_to_keep_cursor_visible_ifn();
}

void Window::update_display_buffer()
{
    m_display_buffer.clear();

    BufferIterator begin = m_buffer.iterator_at(m_position);
    BufferIterator end = m_buffer.iterator_at(m_position +
                                              BufferCoord(m_dimensions.line, m_dimensions.column+1));
    if (begin == end)
        return;

    m_display_buffer.append(DisplayAtom(DisplayCoord(0,0), begin, end));

    for (auto& filter : m_filters)
    {
        filter(m_display_buffer);
        m_display_buffer.check_invariant();
    }
}

void Window::set_dimensions(const DisplayCoord& dimensions)
{
    m_dimensions = dimensions;
}

void Window::scroll_to_keep_cursor_visible_ifn()
{
    check_invariant();

    DisplayCoord cursor = line_and_column_at(m_selections.back().last());
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
    BufferCoord cursor = m_buffer.line_and_column_at(m_selections.back().last());
    std::ostringstream oss;
    oss << m_buffer.name();
    if (m_buffer.is_modified())
        oss << " [+]";
    oss << " -- " << cursor.line+1 << "," << cursor.column+1
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
        case Mode::AppendAtLineEnd:
            pos = sel.end() - 1;
            while (not pos.is_end() and *pos != '\n')
                ++pos;
            if (mode == Mode::OpenLineBelow)
            {
                ++pos;
                window.m_buffer.insert(pos, "\n");
            }
            break;

        case Mode::OpenLineAbove:
        case Mode::InsertAtLineBegin:
            pos = sel.begin();
            while (not pos.is_begin() and *pos != '\n')
                --pos;
            if (mode == Mode::OpenLineAbove)
                window.m_buffer.insert(pos, "\n");
            ++pos;
            break;
        }
        sel = Selection(pos, pos);
    }
}

IncrementalInserter::~IncrementalInserter()
{
    move_cursor(DisplayCoord(0, -1));
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
    move_cursor(DisplayCoord(0, -1));
    m_window.erase_noundo();
}

void IncrementalInserter::move_cursor(const DisplayCoord& offset)
{
    for (auto& sel : m_window.m_selections)
    {
        DisplayCoord pos = m_window.line_and_column_at(sel.last());
        BufferIterator it = m_window.iterator_at(pos + offset);
        sel = Selection(it, it);
    }
}

}
