#include "window.hh"

#include "assert.hh"
#include "highlighter_registry.hh"
#include "hooks_manager.hh"

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

BufferString Selection::capture(size_t index) const
{
    if (index < m_captures.size())
        return m_captures[index];
    return "";
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

    HighlighterRegistry& registry = HighlighterRegistry::instance();

    HooksManager::instance().run_hook("WinCreate", buffer.name(),
                                      Context(*this));

    registry.add_highlighter_to_window(*this, "expand_tabs", HighlighterParameters());
    registry.add_highlighter_to_window(*this, "highlight_selections", HighlighterParameters());
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
        m_buffer.modify(Modification::make_erase(sel.begin(), sel.end()));
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
        m_buffer.modify(Modification::make_insert(sel.begin(), string));
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
        m_buffer.modify(Modification::make_insert(sel.end(), string));
    scroll_to_keep_cursor_visible_ifn();
}

void Window::replace(const std::string& string)
{
    scoped_undo_group undo_group(m_buffer);
    erase_noundo();
    insert_noundo(string);
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
    BufferIterator pos = m_selections.back().last();

    if (*pos == '\n' and not pos.is_begin() and *(pos-1) != '\n')
        --pos;

    Selection sel = Selection(pos, pos);
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

void Window::multi_select(const MultiSelector& selector)
{
    check_invariant();

    SelectionList new_selections;
    for (auto& sel : m_selections)
    {
        SelectionList selections = selector(sel);
        std::copy(selections.begin(), selections.end(),
                  std::back_inserter(new_selections));
    }
    if (not new_selections.empty())
    {
        m_selections = std::move(new_selections);
        scroll_to_keep_cursor_visible_ifn();
    }
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
                                              BufferCoord(m_dimensions.line, m_dimensions.column))+1;
    if (begin == end)
        return;

    m_display_buffer.append(DisplayAtom(DisplayCoord(0,0), begin, end));

    for (auto& highlighter : m_highlighters)
    {
        highlighter.second(m_display_buffer);
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

void Window::add_highlighter(HighlighterAndId&& highlighter)
{
    if (m_highlighters.contains(highlighter.first))
        throw id_not_unique(highlighter.first);
    m_highlighters.append(std::forward<HighlighterAndId>(highlighter));
}

void Window::remove_highlighter(const std::string& id)
{
    m_highlighters.remove(id);
}

CandidateList Window::complete_highlighterid(const std::string& prefix,
                                             size_t cursor_pos)
{
    return m_highlighters.complete_id<str_to_str>(prefix, cursor_pos);
}

void Window::add_filter(FilterAndId&& filter)
{
    if (m_filters.contains(filter.first))
        throw id_not_unique(filter.first);
    m_filters.append(std::forward<FilterAndId>(filter));
}

void Window::remove_filter(const std::string& id)
{
    m_filters.remove(id);
}

CandidateList Window::complete_filterid(const std::string& prefix,
                                        size_t cursor_pos)
{
    return m_filters.complete_id<str_to_str>(prefix, cursor_pos);
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
        DynamicBufferIterator pos;
        switch (mode)
        {
        case Mode::Insert: pos = sel.begin(); break;
        case Mode::Append: pos = sel.end(); break;
        case Mode::Change: pos = sel.begin(); break;

        case Mode::OpenLineBelow:
        case Mode::AppendAtLineEnd:
            pos = m_window.m_buffer.iterator_at_line_end(sel.end() - 1) - 1;
            if (mode == Mode::OpenLineBelow)
                apply(Modification::make_insert(pos, "\n"));
            break;

        case Mode::OpenLineAbove:
        case Mode::InsertAtLineBegin:
            pos = m_window.m_buffer.iterator_at_line_begin(sel.begin());
            if (mode == Mode::OpenLineAbove)
                apply(Modification::make_insert(--pos, "\n"));
            break;
        }
        sel = Selection(pos, pos, sel.captures());
    }
}

IncrementalInserter::~IncrementalInserter()
{
    move_cursor(DisplayCoord(0, -1));
    assert(m_window.m_current_inserter == this);
    m_window.m_current_inserter = nullptr;

    m_window.m_buffer.end_undo_group();
}

void IncrementalInserter::apply(Modification&& modification) const
{
    for (auto filter : m_window.m_filters)
        filter.second(m_window.buffer(), modification);
    m_window.buffer().modify(std::move(modification));
}


void IncrementalInserter::insert(const Window::String& string)
{
    for (auto& sel : m_window.m_selections)
        apply(Modification::make_insert(sel.begin(), string));
}

void IncrementalInserter::insert_capture(size_t index)
{
    for (auto& sel : m_window.m_selections)
        m_window.m_buffer.modify(Modification::make_insert(sel.begin(),
                                                           sel.capture(index)));
    m_window.scroll_to_keep_cursor_visible_ifn();
}

void IncrementalInserter::erase()
{
    for (auto& sel : m_window.m_selections)
    {
        sel = Selection(sel.first() - 1, sel.last() - 1);
        apply(Modification::make_erase(sel.begin(), sel.end()));
    }

    m_window.scroll_to_keep_cursor_visible_ifn();
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
