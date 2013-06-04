#include "window.hh"

#include "assert.hh"
#include "context.hh"
#include "highlighter.hh"
#include "hook_manager.hh"

#include <algorithm>
#include <sstream>

namespace Kakoune
{

// Implementation in highlighters.cc
void highlight_selections(const Window& window, DisplayBuffer& display_buffer);
void expand_tabulations(const Window& window, DisplayBuffer& display_buffer);
void expand_unprintable(const Window& window, DisplayBuffer& display_buffer);

Window::Window(Buffer& buffer)
    : Editor(buffer),
      m_hooks(buffer.hooks()),
      m_options(buffer.options())
{
    Context hook_context{*this};
    m_hooks.run_hook("WinCreate", buffer.name(), hook_context);
    m_options.register_watcher(*this);

    m_builtin_highlighters.append({"tabulations", expand_tabulations});
    m_builtin_highlighters.append({"unprintable", expand_unprintable});
    m_builtin_highlighters.append({"selections",  highlight_selections});

    for (auto& option : m_options.flatten_options())
        on_option_changed(*option);
}

Window::~Window()
{
    m_options.unregister_watcher(*this);
}

void Window::display_selection_at(LineCount line)
{
    if (line >= 0 or line < m_dimensions.line)
    {
        auto cursor_line = main_selection().last().line;
        m_position.line = std::max(0_line, cursor_line - line);
    }
}

void Window::center_selection()
{
    display_selection_at(m_dimensions.line/2_line);
}

void Window::scroll(LineCount offset)
{
    m_position.line = std::max(0_line, m_position.line + offset);
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
        BufferCoord limit{buffer_line+1, 0};
        auto begin = std::min(buffer().char_advance(buffer_line, m_position.column), limit);
        auto end   = std::min(buffer().char_advance(begin, m_dimensions.column), limit);

        lines.push_back(DisplayLine(buffer_line));
        lines.back().push_back(DisplayAtom(AtomContent(buffer(), begin, end)));
    }

    m_display_buffer.compute_range();
    m_highlighters(*this, m_display_buffer);
    m_builtin_highlighters(*this, m_display_buffer);
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

static LineCount adapt_view_pos(LineCount line, LineCount offset,
                                LineCount view_pos, LineCount view_size,
                                LineCount buffer_size)
{
    if (line - offset < view_pos)
        return std::max(0_line, line - offset);
    else if (line + offset >= view_pos + view_size)
        return std::min(buffer_size - view_size,
                        line + offset - (view_size - 1));
    return view_pos;
}

void Window::scroll_to_keep_cursor_visible_ifn()
{
    const auto& first = main_selection().first();
    const auto& last  = main_selection().last();

    const LineCount offset = std::min<LineCount>(options()["scrolloff"].get<int>(),
                                                 (m_dimensions.line - 1) / 2);

    // scroll lines if needed, try to get as much of the selection visible as possible
    m_position.line = adapt_view_pos(first.line, offset, m_position.line,
                                     m_dimensions.line, buffer().line_count());
    m_position.line = adapt_view_pos(last.line,  offset, m_position.line,
                                     m_dimensions.line, buffer().line_count());

    // highlight only the line containing the cursor
    DisplayBuffer display_buffer;
    DisplayBuffer::LineList& lines = display_buffer.lines();
    lines.push_back(DisplayLine(last.line));

    lines.back().push_back(DisplayAtom(AtomContent(buffer(), last.line, last.line+1)));

    display_buffer.compute_range();
    m_highlighters(*this, display_buffer);
    m_builtin_highlighters(*this, display_buffer);

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
            column += atom.content.length();

            CharCount first_col = first.line == last.line ?
                                  buffer().char_distance(last.line, first) : 0_char;
            if (first_col < m_position.column)
                m_position.column = first_col;
            else if (column >= m_position.column + m_dimensions.column)
                m_position.column = column - (m_dimensions.column - 1);

            CharCount last_col = buffer().char_distance(last.line, last);
            if (last_col < m_position.column)
                m_position.column = last_col;
            else if (column >= m_position.column + m_dimensions.column)
                m_position.column = column - (m_dimensions.column - 1);

            return;
        }
        column += atom.content.content().char_length();
    }
    if (not buffer().is_end(last))
    {
        // the cursor should always be visible.
        kak_assert(false);
    }
}

DisplayCoord Window::display_position(const BufferCoord& coord)
{
    DisplayCoord res{0,0};
    for (auto& line : m_display_buffer.lines())
    {
        if (line.buffer_line() == coord.line)
        {
            for (auto& atom : line)
            {
                auto& content = atom.content;
                if (content.has_buffer_range() and
                    coord >= content.begin() and coord < content.end())
                {
                    res.column += utf8::distance(buffer().iterator_at(content.begin()),
                                                 buffer().iterator_at(coord));
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
