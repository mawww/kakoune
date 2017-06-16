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
void setup_builtin_highlighters(HighlighterGroup& group);

Window::Window(Buffer& buffer)
    : Scope(buffer),
      m_buffer(&buffer),
      m_highlighters{HighlightPass::All},
      m_builtin_highlighters{HighlightPass::All}
{
    run_hook_in_own_context("WinCreate", buffer.name());

    options().register_watcher(*this);

    setup_builtin_highlighters(m_builtin_highlighters);

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
    const DisplaySetup setup = compute_display_setup(context);

    m_position = setup.window_pos;
    m_range = setup.window_range;

    const int tabstop = context.options()["tabstop"].get<int>();
    for (LineCount line = 0; line < m_range.line; ++line)
    {
        LineCount buffer_line = m_position.line + line;
        if (buffer_line >= buffer().line_count())
            break;
        auto beg_byte = get_byte_to_column(buffer(), tabstop, {buffer_line, m_position.column});
        auto end_byte = setup.full_lines ?
            buffer()[buffer_line].length()
          : get_byte_to_column(buffer(), tabstop, {buffer_line, m_position.column + m_range.column});

        lines.emplace_back(AtomList{ {buffer(), {buffer_line, beg_byte}, {buffer_line, end_byte}} });
    }

    m_display_buffer.compute_range();
    BufferRange range{{0,0}, buffer().end_coord()};
    for (auto pass : { HighlightPass::Wrap, HighlightPass::Move, HighlightPass::Colorize })
        m_highlighters.highlight(context, pass, m_display_buffer, range);
    for (auto pass : { HighlightPass::Wrap, HighlightPass::Move, HighlightPass::Colorize })
        m_builtin_highlighters.highlight(context, pass, m_display_buffer, range);

    m_display_buffer.optimize();

    m_last_setup = build_setup(context);

    if (profile and not (buffer().flags() & Buffer::Flags::Debug))
    {
        using namespace std::chrono;
        auto duration = duration_cast<microseconds>(Clock::now() - start_time);
        write_to_debug_buffer(format("window display update for '{}' took {} us",
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

DisplaySetup Window::compute_display_setup(const Context& context)
{
    DisplayCoord offset = options()["scrolloff"].get<DisplayCoord>();
    offset.line = std::min(offset.line, (m_dimensions.line + 1) / 2);
    offset.column = std::min(offset.column, (m_dimensions.column + 1) / 2);

    const int tabstop = context.options()["tabstop"].get<int>();
    const auto& cursor = context.selections().main().cursor();

    // Ensure cursor line is visible
    if (cursor.line - offset.line < m_position.line)
        m_position.line = std::max(0_line, cursor.line - offset.line);
    if (cursor.line + offset.line >= m_position.line + m_dimensions.line)
        m_position.line = std::min(buffer().line_count()-1, cursor.line + offset.line - m_dimensions.line + 1);

    DisplaySetup setup{
        m_position,
        m_dimensions,
        {cursor.line - m_position.line,
         get_column(buffer(), tabstop, cursor) - m_position.column},
        offset,
        false
    };
    for (auto pass : { HighlightPass::Move, HighlightPass::Wrap })
        m_highlighters.compute_display_setup(context, pass, setup);
    for (auto pass : { HighlightPass::Move, HighlightPass::Wrap })
        m_builtin_highlighters.compute_display_setup(context, pass, setup);

    // now ensure the cursor column is visible
    {
        auto underflow = setup.cursor_pos.column - setup.scroll_offset.column;
        if (underflow < 0)
        {
            setup.window_pos.column += underflow;
            setup.cursor_pos.column -= underflow;
        }
        auto overflow = setup.cursor_pos.column + setup.scroll_offset.column - setup.window_range.column + 1;
        if (overflow > 0)
        {
            setup.window_pos.column += overflow;
            setup.cursor_pos.column -= overflow;
        }
    }

    return setup;
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
                column += utf8::column_distance(get_iterator(buffer, atom.begin()),
                                                get_iterator(buffer, coord));
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
                    utf8::advance(get_iterator(buffer, atom.begin()),
                                  get_iterator(buffer, range.end),
                                  std::max(0_col, column)).coord());
             return buffer.clamp(atom.begin());
        }
        column -= len;
    }
    return buffer.clamp(buffer.prev(range.end));
}
}

Optional<DisplayCoord> Window::display_position(BufferCoord coord) const
{
    LineCount l = 0;
    for (auto& line : m_display_buffer.lines())
    {
        auto& range = line.range();
        if (range.begin <= coord and coord < range.end)
            return DisplayCoord{l, find_display_column(line, buffer(), coord)};
        ++l;
    }
    return {};
}

BufferCoord Window::buffer_coord(DisplayCoord coord) const
{
    if (m_display_buffer.lines().empty())
        return {0,0};
    if (coord <= 0_line)
        coord = {0,0};
    if ((size_t)coord.line >= m_display_buffer.lines().size())
        coord = DisplayCoord{(int)m_display_buffer.lines().size()-1, INT_MAX};

    return find_buffer_coord(m_display_buffer.lines()[(int)coord.line],
                             buffer(), coord.column);
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
