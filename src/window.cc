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
        lines.push_back(DisplayLine(buffer_line));
        lines.back().push_back(DisplayAtom(AtomContent(buffer(), buffer_line, buffer_line+1)));
    }

    m_display_buffer.compute_range();
    m_highlighters(*this, m_display_buffer);
    m_builtin_highlighters(*this, m_display_buffer);

    // cut the start of the line before m_position.column
    for (auto& line : lines)
        line.trim(m_position.column, m_dimensions.column);
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
    auto cursor = buffer().iterator_at(last);
    CharCount buffer_column = 0;
    CharCount non_buffer_column = 0;
    for (auto& atom : lines.back())
    {
        if (atom.content.has_buffer_range())
        {
            if (atom.content.begin() <= last and atom.content.end() > last)
            {
                if (buffer_column < m_position.column)
                    m_position.column = buffer_column;

                auto last_column = buffer_column + atom.content.length();
                if (last_column >= m_position.column + m_dimensions.column - non_buffer_column)
                    m_position.column = last_column - m_dimensions.column + non_buffer_column;

                return;
            }
            buffer_column += atom.content.length();
        }
        else
            non_buffer_column += atom.content.length();
    }
    if (not buffer().is_end(last))
    {
        // the cursor should always be visible.
        kak_assert(false);
    }
}

namespace
{
CharCount find_display_column(const DisplayLine& line, const Buffer& buffer,
                              const BufferCoord& coord)
{
    kak_assert(coord.line == line.buffer_line());
    CharCount column = 0;
    for (auto& atom : line)
    {
        auto& content = atom.content;
        if (content.has_buffer_range() and
            coord >= content.begin() and coord < content.end())
        {
            if (content.type() == AtomContent::BufferRange)
                column += utf8::distance(buffer.iterator_at(content.begin()),
                                         buffer.iterator_at(coord));
            return column;
        }
        column += content.length();
    }
    return column;
}

BufferCoord find_buffer_coord(const DisplayLine& line, const Buffer& buffer,
                              CharCount column)
{
    LineCount l = line.buffer_line();
    for (auto& atom : line)
    {
        auto& content = atom.content;
        CharCount len = content.length();
        if (content.has_buffer_range() and column < len)
        {
            if (content.type() == AtomContent::BufferRange)
                return utf8::advance(buffer.iterator_at(content.begin()), buffer.iterator_at(l+1),
                                     std::max(0_char, column)).coord();
             return content.begin();
         }
        column -= len;
    }
    return buffer.clamp({l, buffer[l].length()});
}
}

DisplayCoord Window::display_position(const BufferCoord& coord)
{
    LineCount l = 0;
    for (auto& line : m_display_buffer.lines())
    {
        if (line.buffer_line() == coord.line)
            return {l, find_display_column(line, buffer(), coord)};
        ++l;
    }
    return { 0, 0 };
}

BufferCoord Window::offset_coord(const BufferCoord& coord, LineCount offset)
{
    auto line = clamp(coord.line + offset, 0_line, buffer().line_count()-1);
    DisplayBuffer display_buffer;
    DisplayBuffer::LineList& lines = display_buffer.lines();
    {
        lines.emplace_back(coord.line);
        lines.back().push_back({AtomContent(buffer(), coord.line, coord.line+1)});
        lines.emplace_back(line);
        lines.back().push_back({AtomContent(buffer(), line, line+1)});
    }
    display_buffer.compute_range();
    m_highlighters(*this, display_buffer);
    m_builtin_highlighters(*this, display_buffer);

    CharCount column = find_display_column(lines[0], buffer(), coord);
    return find_buffer_coord(lines[1], buffer(), column);
}

void Window::on_option_changed(const Option& option)
{
    String desc = option.name() + "=" + option.get_as_string();
    Context hook_context{*this};
    m_hooks.run_hook("WinSetOption", desc, hook_context);
}

}
