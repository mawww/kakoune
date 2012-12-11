#include "window.hh"

#include "assert.hh"
#include "highlighter.hh"
#include "hook_manager.hh"
#include "context.hh"

#include <algorithm>
#include <sstream>

namespace Kakoune
{

Window::Window(Buffer& buffer)
    : Editor(buffer),
      m_hooks(buffer.hooks()),
      m_options(buffer.options())
{
    HighlighterRegistry& registry = HighlighterRegistry::instance();

    m_hooks.run_hook("WinCreate", buffer.name(), Context(*this));
    m_options.register_watcher(*this);

    m_highlighters.append(registry["expand_tabs"](*this, {}));
    m_highlighters.append(registry["highlight_selections"](*this, {}));

    for (auto& option : m_options.flatten_options())
        on_option_changed(option.first, option.second);
}

Window::~Window()
{
    m_options.unregister_watcher(*this);
}

void Window::center_selection()
{
    BufferIterator cursor = selections().back().last();
    m_position.line = std::max(0_line, cursor.line() - m_dimensions.line/2_line);
}

void Window::update_display_buffer()
{
    scroll_to_keep_cursor_visible_ifn();

    DisplayBuffer::LineList& lines = m_display_buffer.lines();
    lines.clear();

    for (LineCount line = 0; line < m_dimensions.line; ++line)
    {
        LineCount buffer_line = m_position.line + line;
        if (buffer_line >= buffer().line_count())
            break;
        BufferIterator line_begin = buffer().iterator_at_line_begin(buffer_line);
        BufferIterator line_end   = buffer().iterator_at_line_end(buffer_line);

        BufferIterator begin = utf8::advance(line_begin, line_end, (int)m_position.column);
        BufferIterator end   = utf8::advance(begin,      line_end, (int)m_dimensions.column);

        lines.push_back(DisplayLine(buffer_line));
        lines.back().push_back(DisplayAtom(AtomContent(begin, end)));
    }

    m_display_buffer.compute_range();
    m_highlighters(m_display_buffer);
    m_display_buffer.optimize();

    m_timestamp = buffer().timestamp();
}

void Window::set_position(const DisplayCoord& position)
{
    m_position.line = std::max(0_line, position.line);
    m_position.column = std::max(0_char, position.column);
}

void Window::set_dimensions(const DisplayCoord& dimensions)
{
    m_dimensions = dimensions;
}

void Window::scroll_to_keep_cursor_visible_ifn()
{
    const BufferIterator cursor = selections().back().last();

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
    CharCount column = 0;
    for (auto& atom : lines.back())
    {
        if (atom.content.has_buffer_range() and
            atom.content.begin() <= cursor and atom.content.end() > cursor)
        {
            if (atom.content.type() == AtomContent::BufferRange)
                column += utf8::distance(atom.content.begin(), cursor);
            else
                column += atom.content.content().char_length();

            CharCount cursor_col = utf8::distance(line_begin, cursor);
            // we could early out on this, but having scrolling left
            // faster than not scrolling at all is not really useful.
            if (cursor_col < m_position.column)
                m_position.column = cursor_col;
            else if (column >= m_position.column + m_dimensions.column)
                m_position.column = column - (m_dimensions.column - 1);

            return;
        }
        column += atom.content.content().char_length();
    }
    if (cursor != buffer().end())
    {
        // the cursor should always be visible.
        assert(false);
    }
}

DisplayCoord Window::display_position(const BufferIterator& iterator)
{
    DisplayCoord res{0,0};
    for (auto& line : m_display_buffer.lines())
    {
        if (line.buffer_line() == iterator.line())
        {
            for (auto& atom : line)
            {
                auto& content = atom.content;
                if (content.has_buffer_range() and
                    iterator >= content.begin() and iterator < content.end())
                {
                    res.column += iterator - content.begin();
                    return res;
                }
                res.column += content.length();
            }
        }
        ++res.line;
    }
    return { 0, 0 };
}

String Window::status_line() const
{
    BufferCoord cursor = selections().back().last().coord();
    std::ostringstream oss;
    oss << buffer().name();
    if (buffer().is_modified())
        oss << " [+]";
    oss << " -- " << (int)cursor.line+1 << "," << (int)cursor.column+1
        << " -- " << selections().size() << " sel -- ";
    if (is_editing())
        oss << "[Insert]";
    return oss.str();
}

void Window::on_incremental_insertion_end()
{
    DynamicSelectionList backup(buffer(), selections());
    hooks().run_hook("InsertEnd", "", Context(*this));
    select((SelectionList)backup);
}

void Window::on_option_changed(const String& name, const Option& option)
{
    String desc = name + "=" + option.as_string();
    m_hooks.run_hook("WinSetOption", desc, Context(*this));
}

}
