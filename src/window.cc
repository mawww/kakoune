#include "window.hh"

#include "assert.hh"
#include "clock.hh"
#include "context.hh"
#include "highlighter.hh"
#include "hook_manager.hh"
#include "input_handler.hh"
#include "client.hh"
#include "buffer_utils.hh"
#include "option.hh"

#include <algorithm>
#include <sstream>

namespace Kakoune
{

// Implementation in highlighters.cc
void highlight_selections(const Context& context, HighlightFlags flags, DisplayBuffer& display_buffer, BufferRange range);
void expand_tabulations(const Context& context, HighlightFlags flags, DisplayBuffer& display_buffer, BufferRange range);
void expand_unprintable(const Context& context, HighlightFlags flags, DisplayBuffer& display_buffer, BufferRange range);

Window::Window(Buffer& buffer)
    : Scope(buffer),
      m_buffer(&buffer)
{
    run_hook_in_own_context("WinCreate", buffer.name());

    options().register_watcher(*this);

    m_builtin_highlighters.add_child({"tabulations"_str, make_simple_highlighter(expand_tabulations)});
    m_builtin_highlighters.add_child({"unprintable"_str, make_simple_highlighter(expand_unprintable)});
    m_builtin_highlighters.add_child({"selections"_str,  make_simple_highlighter(highlight_selections)});

    for (auto& option : options().flatten_options())
        on_option_changed(*option);
}

Window::~Window()
{
    run_hook_in_own_context("WinClose", buffer().name());
    options().unregister_watcher(*this);
}

void Window::scroll(LineCount offset)
{
    m_position.line = std::max(0_line, m_position.line + offset);
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

void Window::scroll(ColumnCount offset)
{
    m_position.column = std::max(0_col, m_position.column + offset);
}

void Window::display_column_at(ColumnCount buffer_column, ColumnCount display_column)
{
    if (display_column >= 0 or display_column < m_dimensions.column)
        m_position.column = std::max(0_col, buffer_column - display_column);
}

void Window::center_column(ColumnCount buffer_column)
{
    display_column_at(buffer_column, m_dimensions.column/2_col);
}

Window::Setup Window::build_setup(const Context& context) const
{
    Vector<BufferRange, MemoryDomain::Display> selections;
    for (auto& sel : context.selections())
        selections.push_back({sel.cursor(), sel.anchor()});

    return { m_position, m_dimensions,
             context.buffer().timestamp(),
             context.selections().main_index(),
             std::move(selections) };
}

bool Window::needs_redraw(const Context& context) const
{
    auto& selections = context.selections();

    if (m_position != m_last_setup.position or
        m_dimensions != m_last_setup.dimensions or
        context.buffer().timestamp() != m_last_setup.timestamp or
        selections.main_index() != m_last_setup.main_selection or
        selections.size() != m_last_setup.selections.size())
        return true;

    for (int i = 0; i < selections.size(); ++i)
    {
        if (selections[i].cursor() != m_last_setup.selections[i].begin or
            selections[i].anchor() != m_last_setup.selections[i].end)
            return true;
    }

    return false;
}

const DisplayBuffer& Window::update_display_buffer(const Context& context)
{
    const bool profile = context.options()["debug"].get<DebugFlags>() &
                        DebugFlags::Profile;

    auto start_time = profile ? Clock::now() : Clock::time_point{};

    DisplayBuffer::LineList& lines = m_display_buffer.lines();
    lines.clear();

    if (m_dimensions == DisplayCoord{0,0})
        return m_display_buffer;

    kak_assert(&buffer() == &context.buffer());
    scroll_to_keep_selection_visible_ifn(context);

    for (LineCount line = 0; line < m_dimensions.line; ++line)
    {
        LineCount buffer_line = m_position.line + line;
        if (buffer_line >= buffer().line_count())
            break;
        lines.emplace_back(AtomList{ {buffer(), buffer_line, buffer_line+1} });
    }

    m_display_buffer.compute_range();
    BufferRange range{{0,0}, buffer().end_coord()};
    m_highlighters.highlight(context, HighlightFlags::Highlight, m_display_buffer, range);
    m_builtin_highlighters.highlight(context, HighlightFlags::Highlight, m_display_buffer, range);

    // cut the start of the line before m_position.column
    for (auto& line : lines)
        line.trim(m_position.column, m_dimensions.column, true);
    m_display_buffer.optimize();

    m_last_setup = build_setup(context);

    if (profile and not (buffer().flags() & Buffer::Flags::Debug))
    {
        using namespace std::chrono;
        auto duration = duration_cast<milliseconds>(Clock::now() - start_time);
        write_to_debug_buffer(format("window display update for '{}' took {} ms",
                                     buffer().display_name(), (size_t)duration.count()));
    }

    return m_display_buffer;
}

void Window::set_position(DisplayCoord position)
{
    m_position.line = std::max(0_line, position.line);
    m_position.column = std::max(0_col, position.column);
}

void Window::set_dimensions(DisplayCoord dimensions)
{
    if (m_dimensions != dimensions)
    {
        m_dimensions = dimensions;
        run_hook_in_own_context("WinResize", format("{}.{}", dimensions.line,
                                                    dimensions.column));
    }
}

static LineCount adapt_view_pos(LineCount line, LineCount offset,
                                LineCount view_pos, LineCount view_size,
                                LineCount buffer_size)
{
    offset = std::min(offset, (view_size + 1) / 2);
    if (line - offset < view_pos)
        return std::max(0_line, line - offset);
    else if (line + offset >= view_pos + view_size)
        return std::max(0_line, line + offset - view_size + 1);
    return view_pos;
}

static ColumnCount adapt_view_pos(const DisplayBuffer& display_buffer, ColumnCount offset,
                                BufferCoord pos, ColumnCount view_pos, ColumnCount view_size)
{
    offset = std::min(offset, (view_size + 1) / 2);
    ColumnCount buffer_column = 0;
    ColumnCount non_buffer_column = 0;
    for (auto& line : display_buffer.lines())
    {
        for (auto& atom : line)
        {
            if (atom.has_buffer_range())
            {
                if (atom.begin() <= pos and atom.end() > pos)
                {
                    ColumnCount pos_beg, pos_end;
                    if (atom.type() == DisplayAtom::Range)
                    {
                        auto& buf = atom.buffer();
                        pos_beg = buffer_column +
                            column_length(buf, atom.begin(), pos);
                        pos_end = pos_beg+1;
                    }
                    else
                    {
                        pos_beg = buffer_column;
                        pos_end = pos_beg + atom.length();
                    }

                    if (pos_beg - offset < view_pos)
                        return std::max(0_col, pos_beg - offset);

                    if (pos_end + offset >= view_pos + view_size - non_buffer_column)
                        return pos_end + offset - view_size + non_buffer_column;
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
    const auto& anchor = selection.anchor();
    const auto& cursor  = selection.cursor();

    const DisplayCoord offset = options()["scrolloff"].get<DisplayCoord>();

    // scroll lines if needed, try to get as much of the selection visible as possible
    m_position.line = adapt_view_pos(anchor.line, offset.line, m_position.line,
                                     m_dimensions.line, buffer().line_count());
    m_position.line = adapt_view_pos(cursor.line,  offset.line, m_position.line,
                                     m_dimensions.line, buffer().line_count());

    // highlight only the line containing the cursor
    DisplayBuffer display_buffer;
    DisplayBuffer::LineList& lines = display_buffer.lines();
    lines.emplace_back(AtomList{ {buffer(), cursor.line, cursor.line+1} });

    display_buffer.compute_range();
    BufferRange range{cursor.line, cursor.line + 1};
    m_highlighters.highlight(context, HighlightFlags::MoveOnly, display_buffer, range);
    m_builtin_highlighters.highlight(context, HighlightFlags::MoveOnly, display_buffer, range);

    // now we can compute where the cursor is in display columns
    // (this is only valid if highlighting one line and multiple lines put
    // the cursor in the same position, however I do not find any sane example
    // of highlighters not doing that)
    m_position.column = adapt_view_pos(display_buffer, offset.column,
                                       anchor.line == cursor.line ? anchor : cursor.line,
                                       m_position.column, m_dimensions.column);
    m_position.column = adapt_view_pos(display_buffer, offset.column, cursor,
                                       m_position.column, m_dimensions.column);
}

namespace
{
ColumnCount find_display_column(const DisplayLine& line, const Buffer& buffer,
                                BufferCoord coord)
{
    ColumnCount column = 0;
    for (auto& atom : line)
    {
        if (atom.has_buffer_range() and
            coord >= atom.begin() and coord < atom.end())
        {
            if (atom.type() == DisplayAtom::Range)
                column += column_length(buffer, atom.begin(), coord);
            return column;
        }
        column += atom.length();
    }
    return column;
}

BufferCoord find_buffer_coord(const DisplayLine& line, const Buffer& buffer,
                              ColumnCount column)
{
    auto& range = line.range();
    for (auto& atom : line)
    {
        ColumnCount len = atom.length();
        if (atom.has_buffer_range() and column < len)
        {
            if (atom.type() == DisplayAtom::Range)
                return buffer.clamp(
                    utf8::advance(buffer.iterator_at(atom.begin()),
                                  buffer.iterator_at(range.end),
                                  std::max(0_col, column)).coord());
             return buffer.clamp(atom.begin());
        }
        column -= len;
    }
    return buffer.clamp(buffer.prev(range.end));
}
}

DisplayCoord Window::display_position(BufferCoord coord) const
{
    LineCount l = 0;
    for (auto& line : m_display_buffer.lines())
    {
        auto& range = line.range();
        if (range.begin <= coord and coord < range.end)
            return {l, find_display_column(line, buffer(), coord)};
        ++l;
    }
    return { 0, 0 };
}

BufferCoord Window::buffer_coord(DisplayCoord coord) const
{
    if (m_display_buffer.lines().empty())
        return {0,0};
    if (coord <= 0_line)
        coord = {0,0};
    if ((int)coord.line >= m_display_buffer.lines().size())
        coord = DisplayCoord{(int)m_display_buffer.lines().size()-1, INT_MAX};

    return find_buffer_coord(m_display_buffer.lines()[(int)coord.line],
                             buffer(), coord.column);
}

BufferCoord Window::offset_coord(BufferCoord coord, CharCount offset)
{
    return buffer().offset_coord(coord, offset);
}

BufferCoordAndTarget Window::offset_coord(BufferCoordAndTarget coord, LineCount offset)
{
    auto line = clamp(coord.line + offset, 0_line, buffer().line_count()-1);
    DisplayBuffer display_buffer;
    DisplayBuffer::LineList& lines = display_buffer.lines();
    lines.emplace_back(AtomList{ {buffer(), coord.line, coord.line+1} });
    lines.emplace_back(AtomList{ {buffer(), line, line+1} });
    display_buffer.compute_range();

    BufferRange range{ std::min(line, coord.line), std::max(line,coord.line)+1};

    InputHandler input_handler{{ *m_buffer, Selection{} }, Context::Flags::Transient};
    input_handler.context().set_window(*this);
    m_highlighters.highlight(input_handler.context(), HighlightFlags::MoveOnly, display_buffer, range);
    m_builtin_highlighters.highlight(input_handler.context(), HighlightFlags::MoveOnly, display_buffer, range);

    ColumnCount column = coord.target == -1 ? find_display_column(lines[0], buffer(), coord) : coord.target;
    return { find_buffer_coord(lines[1], buffer(), column), column };
}

void Window::clear_display_buffer()
{
    m_display_buffer = DisplayBuffer{};
}

void Window::on_option_changed(const Option& option)
{
    run_hook_in_own_context("WinSetOption", format("{}={}", option.name(),
                                                   option.get_as_string()));
    // an highlighter might depend on the option, so we need to redraw
    force_redraw();
}


void Window::run_hook_in_own_context(StringView hook_name, StringView param,
                                     String client_name)
{
    if (m_buffer->flags() & Buffer::Flags::NoHooks)
        return;

    InputHandler hook_handler{{ *m_buffer, Selection{} },
                              Context::Flags::Transient,
                              std::move(client_name)};
    hook_handler.context().set_window(*this);
    if (m_client)
        hook_handler.context().set_client(*m_client);

    hooks().run_hook(hook_name, param, hook_handler.context());
}
}
