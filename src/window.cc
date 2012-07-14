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
      m_hook_manager(buffer.hook_manager()),
      m_option_manager(buffer.option_manager())
{
    HighlighterRegistry& registry = HighlighterRegistry::instance();

    m_hook_manager.run_hook("WinCreate", buffer.name(), Context(*this));
    m_option_manager.register_watcher(*this);

    registry.add_highlighter_to_group(*this, m_highlighters, "expand_tabs", HighlighterParameters());
    registry.add_highlighter_to_group(*this, m_highlighters, "highlight_selections", HighlighterParameters());

    for (auto& option : m_option_manager.flatten_options())
        on_option_changed(option.first, option.second);
}

Window::~Window()
{
    m_option_manager.unregister_watcher(*this);
}

void Window::update_display_buffer()
{
    scroll_to_keep_cursor_visible_ifn();

    DisplayBuffer::LineList& lines = m_display_buffer.lines();
    lines.clear();

    for (auto line = 0; line < m_dimensions.line; ++line)
    {
        auto buffer_line = m_position.line + line;
        if (buffer_line >= buffer().line_count())
            break;
        BufferIterator pos        = buffer().iterator_at({ buffer_line, m_position.column });
        BufferIterator line_begin = buffer().iterator_at_line_begin(pos);
        BufferIterator line_end   = buffer().iterator_at_line_end(pos);

        BufferIterator end;
        if (line_end - pos > m_dimensions.column)
            end = pos + m_dimensions.column;
        else
            end = line_end;

        lines.push_back(DisplayLine(buffer_line));
        lines.back().push_back(DisplayAtom(AtomContent(pos,end)));
    }

    m_display_buffer.compute_range();
    m_highlighters(m_display_buffer);
}

void Window::set_dimensions(const DisplayCoord& dimensions)
{
    m_dimensions = dimensions;
}

void Window::scroll_to_keep_cursor_visible_ifn()
{
    BufferIterator cursor = selections().back().last();

    // scroll lines if needed
    if (cursor.line() < m_position.line)
        m_position.line = cursor.line();
    else if (cursor.line() >= m_position.line + m_dimensions.line)
        m_position.line = cursor.line() - (m_dimensions.line - 1);

    // highlight only the line containing the cursor
    DisplayBuffer display_buffer;
    DisplayBuffer::LineList& lines = display_buffer.lines();
    lines.push_back(DisplayLine(cursor.line()));

    BufferIterator line_begin = buffer().iterator_at_line_begin(cursor);
    BufferIterator line_end   = buffer().iterator_at_line_end(cursor);
    lines.back().push_back(DisplayAtom(AtomContent(line_begin, line_end)));

    display_buffer.compute_range();
    m_highlighters(display_buffer);

    // now we can compute where the cursor is in display columns
    // (this is only valid if highlighting one line and multiple lines put
    // the cursor in the same position, however I do not find any sane example
    // of highlighters not doing that)
    int column = 0;
    for (auto& atom : lines.back())
    {
        if (atom.content.has_buffer_range() and
            atom.content.begin() <= cursor and atom.content.end() > cursor)
        {
            if (atom.content.type() == AtomContent::BufferRange)
                column += cursor - atom.content.begin();
            else
                column += atom.content.content().length();

            // we could early out on this, but having scrolling left
            // faster than not scrolling at all is not really useful.
            if (cursor.column() < m_position.column)
                m_position.column = cursor.column();
            else if (column >= m_position.column + m_dimensions.column)
                m_position.column = column - (m_dimensions.column - 1);

            return;
        }
        column += atom.content.content().length();
    }
    // the cursor should always be visible.
    assert(false);
}

String Window::status_line() const
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

void Window::on_option_changed(const String& name, const Option& option)
{
    String desc = name + "=" + option.as_string();
    m_hook_manager.run_hook("WinSetOption", desc, Context(*this));
}

}
