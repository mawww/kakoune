#include "window.hh"

#include "assert.hh"
#include "highlighter_registry.hh"
#include "hook_manager.hh"
#include "context.hh"

#include <algorithm>
#include <sstream>

namespace Kakoune
{

Window::Window(Buffer& buffer)
    : Editor(buffer),
      m_position(0, 0),
      m_dimensions(0, 0),
      m_option_manager(buffer.option_manager())
{
    HighlighterRegistry& registry = HighlighterRegistry::instance();

    GlobalHookManager::instance().run_hook("WinCreate", buffer.name(),
                                            Context(*this));

    registry.add_highlighter_to_window(*this, "expand_tabs", HighlighterParameters());
    registry.add_highlighter_to_window(*this, "highlight_selections", HighlighterParameters());
}

BufferIterator Window::iterator_at(const DisplayCoord& window_pos) const
{
    if (m_display_buffer.begin() == m_display_buffer.end())
        return buffer().begin();

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

    return buffer().iterator_at(m_position + BufferCoord(window_pos));
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
    BufferCoord coord = buffer().line_and_column_at(iterator);
    return DisplayCoord(coord.line - m_position.line,
                        coord.column - m_position.column);
}

void Window::update_display_buffer()
{
    scroll_to_keep_cursor_visible_ifn();

    m_display_buffer.clear();

    BufferIterator begin = buffer().iterator_at(m_position);
    BufferIterator end = buffer().iterator_at(m_position +
                                              BufferCoord(m_dimensions.line, m_dimensions.column))+2;
    if (begin == end)
        return;

    m_display_buffer.append(DisplayAtom(DisplayCoord(0,0), begin, end));

    m_highlighters(m_display_buffer);
    m_display_buffer.check_invariant();
}

void Window::set_dimensions(const DisplayCoord& dimensions)
{
    m_dimensions = dimensions;
}

void Window::scroll_to_keep_cursor_visible_ifn()
{
    DisplayCoord cursor = line_and_column_at(selections().back().last());
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
    BufferCoord cursor = buffer().line_and_column_at(selections().back().last());
    std::ostringstream oss;
    oss << buffer().name();
    if (buffer().is_modified())
        oss << " [+]";
    oss << " -- " << cursor.line+1 << "," << cursor.column+1
        << " -- " << selections().size() << " sel -- ";
    if (is_editing())
        oss << "[Insert]";
    return oss.str();
}

void Window::on_incremental_insertion_end()
{
    push_selections();
    hook_manager().run_hook("InsertEnd", "", Context(*this));
    pop_selections();
}

}
