#include "window.hh"

#include "assert.hh"
#include "highlighter.hh"
#include "hook_manager.hh"
#include "context.hh"

#include <algorithm>
#include <sstream>

namespace Kakoune
{

// Implementation in highlighters.cc
void highlight_selections(const SelectionList& selections, DisplayBuffer& display_buffer);
void expand_tabulations(const OptionManager& options, DisplayBuffer& display_buffer);
void expand_unprintable(DisplayBuffer& display_buffer);

Window::Window(Buffer& buffer)
    : Editor(buffer),
      m_hooks(buffer.hooks()),
      m_options(buffer.options())
{
    Context hook_context{*this};
    m_hooks.run_hook("WinCreate", buffer.name(), hook_context);
    m_options.register_watcher(*this);

    m_builtin_highlighters.append({"tabulations", [this](DisplayBuffer& db) { expand_tabulations(m_options, db); }});
    m_builtin_highlighters.append({"unprintable", expand_unprintable});
    m_builtin_highlighters.append({"selections",  [this](DisplayBuffer& db) { highlight_selections(selections(), db); }});

    for (auto& option : m_options.flatten_options())
        on_option_changed(*option);
}

Window::~Window()
{
    m_options.unregister_watcher(*this);
}

void Window::center_selection()
{
    BufferIterator cursor = main_selection().last();
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
    m_builtin_highlighters(m_display_buffer);
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
    const BufferIterator first = main_selection().first();
    const BufferIterator last  = main_selection().last();

    // scroll lines if needed
    if (first.line() < m_position.line)
        m_position.line = first.line();
    else if (first.line() >= m_position.line + m_dimensions.line)
        m_position.line = first.line() - (m_dimensions.line - 1);

    if (last.line() < m_position.line)
        m_position.line = last.line();
    else if (last.line() >= m_position.line + m_dimensions.line)
        m_position.line = last.line() - (m_dimensions.line - 1);

    // highlight only the line containing the cursor
    DisplayBuffer display_buffer;
    DisplayBuffer::LineList& lines = display_buffer.lines();
    lines.push_back(DisplayLine(last.line()));

    BufferIterator line_begin = buffer().iterator_at_line_begin(last);
    BufferIterator line_end   = buffer().iterator_at_line_end(last);
    lines.back().push_back(DisplayAtom(AtomContent(line_begin, line_end)));

    display_buffer.compute_range();
    m_highlighters(display_buffer);
    m_builtin_highlighters(display_buffer);

    // now we can compute where the cursor is in display columns
    // (this is only valid if highlighting one line and multiple lines put
    // the cursor in the same position, however I do not find any sane example
    // of highlighters not doing that)
    CharCount column = 0;
    for (auto& atom : lines.back())
    {
        if (atom.content.has_buffer_range() and
            atom.content.begin() <= last and atom.content.end() > last)
        {
            if (atom.content.type() == AtomContent::BufferRange)
                column += utf8::distance(atom.content.begin(), last);
            else
                column += atom.content.content().char_length();

            CharCount first_col = first.line() == last.line() ?
                                  utf8::distance(line_begin, first) : 0_char;
            if (first_col < m_position.column)
                m_position.column = first_col;
            else if (column >= m_position.column + m_dimensions.column)
                m_position.column = column - (m_dimensions.column - 1);

            CharCount last_col = utf8::distance(line_begin, last);
            if (last_col < m_position.column)
                m_position.column = last_col;
            else if (column >= m_position.column + m_dimensions.column)
                m_position.column = column - (m_dimensions.column - 1);

            return;
        }
        column += atom.content.content().char_length();
    }
    if (last != buffer().end())
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
                    res.column += utf8::distance(content.begin(), iterator);
                    return res;
                }
                res.column += content.length();
            }
        }
        ++res.line;
    }
    return { 0, 0 };
}

void Window::on_option_changed(const Option& option)
{
    String desc = option.name() + "=" + option.get_as_string();
    Context hook_context{*this};
    m_hooks.run_hook("WinSetOption", desc, hook_context);
}

}
