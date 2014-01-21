#include "window.hh"

#include "assert.hh"
#include "context.hh"
#include "highlighter.hh"
#include "hook_manager.hh"
#include "client.hh"

#include <algorithm>
#include <sstream>

namespace Kakoune
{

// Implementation in highlighters.cc
void highlight_selections(const Context& context, HighlightFlags flags, DisplayBuffer& display_buffer);
void expand_tabulations(const Context& context, HighlightFlags flags, DisplayBuffer& display_buffer);
void expand_unprintable(const Context& context, HighlightFlags flags, DisplayBuffer& display_buffer);

Window::Window(Buffer& buffer)
    : m_buffer(&buffer),
      m_hooks(buffer.hooks()),
      m_options(buffer.options()),
      m_keymaps(buffer.keymaps())
{
    InputHandler hook_handler{*m_buffer, SelectionList{ {} } };
    hook_handler.context().set_window(*this);
    m_hooks.run_hook("WinCreate", buffer.name(), hook_handler.context());
    m_options.register_watcher(*this);

    m_builtin_highlighters.append({"tabulations", expand_tabulations});
    m_builtin_highlighters.append({"unprintable", expand_unprintable});
    m_builtin_highlighters.append({"selections",  highlight_selections});

    for (auto& option : m_options.flatten_options())
        on_option_changed(*option);
}

Window::~Window()
{
    InputHandler hook_handler{*m_buffer, SelectionList{ {} } };
    hook_handler.context().set_window(*this);
    m_hooks.run_hook("WinClose", buffer().name(), hook_handler.context());
    m_options.unregister_watcher(*this);
}

void Window::display_line_at(LineCount buffer_line, LineCount display_line)
{
    if (display_line >= 0 or display_line < m_dimensions.line)
        m_position.line = std::max(0_line, buffer_line - display_line);
}

void Window::center_line(LineCount buffer_line)
{
    display_line_at(buffer_line, m_dimensions.line/2_line);
}

void Window::scroll(LineCount offset)
{
    m_position.line = std::max(0_line, m_position.line + offset);
}

void Window::scroll(CharCount offset)
{
    m_position.column = std::max(0_char, m_position.column + offset);
}

void Window::update_display_buffer(const Context& context)
{
    kak_assert(&buffer() == &context.buffer());
    scroll_to_keep_selection_visible_ifn(context);

    DisplayBuffer::LineList& lines = m_display_buffer.lines();
    lines.clear();

    for (LineCount line = 0; line < m_dimensions.line; ++line)
    {
        LineCount buffer_line = m_position.line + line;
        if (buffer_line >= buffer().line_count())
            break;
        lines.emplace_back(AtomList{ {buffer(), buffer_line, buffer_line+1} });
    }

    m_display_buffer.compute_range();
    m_highlighters(context, HighlightFlags::Highlight, m_display_buffer);
    m_builtin_highlighters(context, HighlightFlags::Highlight, m_display_buffer);

    // cut the start of the line before m_position.column
    for (auto& line : lines)
        line.trim(m_position.column, m_dimensions.column);
    m_display_buffer.optimize();

    m_timestamp = buffer().timestamp();
}

void Window::set_position(DisplayCoord position)
{
    m_position.line = std::max(0_line, position.line);
    m_position.column = std::max(0_char, position.column);
}

void Window::set_dimensions(DisplayCoord dimensions)
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

static CharCount adapt_view_pos(const DisplayBuffer& display_buffer,
                                BufferCoord pos, CharCount view_pos, CharCount view_size)
{
    CharCount buffer_column = 0;
    CharCount non_buffer_column = 0;
    for (auto& line : display_buffer.lines())
    {
        for (auto& atom : line)
        {
            if (atom.has_buffer_range())
            {
                if (atom.begin() <= pos and atom.end() > pos)
                {
                    CharCount pos_beg, pos_end;
                    if (atom.type() == DisplayAtom::BufferRange)
                    {
                        auto& buf = atom.buffer();
                        pos_beg = buffer_column
                                + utf8::distance(buf.iterator_at(atom.begin()),
                                                 buf.iterator_at(pos));
                        pos_end = pos_beg+1;
                    }
                    else
                    {
                        pos_beg = buffer_column;
                        pos_end = pos_beg + atom.length();
                    }

                    if (pos_beg < view_pos)
                        return pos_beg;

                    if (pos_end >= view_pos + view_size - non_buffer_column)
                        return pos_end - view_size + non_buffer_column;
                }
                buffer_column += atom.length();
            }
            else
                non_buffer_column += atom.length();
        }
    }
    return view_pos;
}

void Window::scroll_to_keep_selection_visible_ifn(const Context& context)
{
    auto& selection = context.selections().main();
    const auto& first = selection.first();
    const auto& last  = selection.last();

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
    lines.emplace_back(AtomList{ {buffer(), last.line, last.line+1} });

    display_buffer.compute_range();
    m_highlighters(context, HighlightFlags::MoveOnly, display_buffer);
    m_builtin_highlighters(context, HighlightFlags::MoveOnly, display_buffer);

    // now we can compute where the cursor is in display columns
    // (this is only valid if highlighting one line and multiple lines put
    // the cursor in the same position, however I do not find any sane example
    // of highlighters not doing that)
    m_position.column = adapt_view_pos(display_buffer,
                                       first.line == last.line ? first : last.line,
                                       m_position.column, m_dimensions.column);
    m_position.column = adapt_view_pos(display_buffer, last,
                                       m_position.column, m_dimensions.column);
}

namespace
{
CharCount find_display_column(const DisplayLine& line, const Buffer& buffer,
                              BufferCoord coord)
{
    CharCount column = 0;
    for (auto& atom : line)
    {
        if (atom.has_buffer_range() and
            coord >= atom.begin() and coord < atom.end())
        {
            if (atom.type() == DisplayAtom::BufferRange)
                column += utf8::distance(buffer.iterator_at(atom.begin()),
                                         buffer.iterator_at(coord));
            return column;
        }
        column += atom.length();
    }
    return column;
}

BufferCoord find_buffer_coord(const DisplayLine& line, const Buffer& buffer,
                              CharCount column)
{
    auto& range = line.range();
    for (auto& atom : line)
    {
        CharCount len = atom.length();
        if (atom.has_buffer_range() and column < len)
        {
            if (atom.type() == DisplayAtom::BufferRange)
                return utf8::advance(buffer.iterator_at(atom.begin()), buffer.iterator_at(range.second),
                                     std::max(0_char, column)).coord();
             return atom.begin();
         }
        column -= len;
    }
    return buffer.clamp(buffer.prev(range.second));
}
}

DisplayCoord Window::display_position(BufferCoord coord)
{
    LineCount l = 0;
    for (auto& line : m_display_buffer.lines())
    {
        auto& range = line.range();
        if (range.first <= coord and coord < range.second)
            return {l, find_display_column(line, buffer(), coord)};
        ++l;
    }
    return { 0, 0 };
}

BufferCoord Window::offset_coord(BufferCoord coord, CharCount offset)
{
    return buffer().offset_coord(coord, offset);
}

BufferCoord Window::offset_coord(BufferCoord coord, LineCount offset)
{
    auto line = clamp(coord.line + offset, 0_line, buffer().line_count()-1);
    DisplayBuffer display_buffer;
    DisplayBuffer::LineList& lines = display_buffer.lines();
    lines.emplace_back(AtomList{ {buffer(), coord.line, coord.line+1} });
    lines.emplace_back(AtomList{ {buffer(), line, line+1} });
    display_buffer.compute_range();

    InputHandler hook_handler{*m_buffer, SelectionList{ {} } };
    hook_handler.context().set_window(*this);
    m_highlighters(hook_handler.context(), HighlightFlags::MoveOnly, display_buffer);
    m_builtin_highlighters(hook_handler.context(), HighlightFlags::MoveOnly, display_buffer);

    CharCount column = find_display_column(lines[0], buffer(), coord);
    return find_buffer_coord(lines[1], buffer(), column);
}

void Window::on_option_changed(const Option& option)
{
    String desc = option.name() + "=" + option.get_as_string();
    InputHandler hook_handler{*m_buffer, SelectionList{ {} } };
    hook_handler.context().set_window(*this);
    m_hooks.run_hook("WinSetOption", desc, hook_handler.context());

    // an highlighter might depend on the option, so we need to redraw
    forget_timestamp();
}

}
