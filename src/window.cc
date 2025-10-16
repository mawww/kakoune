#include "window.hh"

#include "assert.hh"
#include "buffer.hh"
#include "buffer_utils.hh"
#include "context.hh"
#include "highlighter.hh"
#include "hook_manager.hh"
#include "input_handler.hh"
#include "client.hh"
#include "debug.hh"
#include "option.hh"
#include "option_types.hh"
#include "profile.hh"

#include <algorithm>

namespace Kakoune
{

// Implementation in highlighters.cc
void setup_builtin_highlighters(HighlighterGroup& group);

Window::Window(Buffer& buffer)
    : Scope(buffer),
      m_buffer(&buffer),
      m_builtin_highlighters{highlighters()}
{
    run_hook_in_own_context(Hook::WinCreate, buffer.name());

    options().register_watcher(*this);

    setup_builtin_highlighters(m_builtin_highlighters.group());

    // gather as on_option_changed can mutate the option managers
    for (auto& option : options().flatten_options()
                      | transform([](auto& ptr) { return ptr.get(); })
                      | gather<Vector<const Option*>>())
        on_option_changed(*option);
}

Window::~Window()
{
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

static uint32_t compute_faces_hash(const FaceRegistry& faces)
{
    uint32_t hash = 0;
    for (auto&& face : faces.flatten_faces() | transform(&FaceRegistry::FaceMap::Item::value))
        hash = combine_hash(hash, face.base.empty() ? hash_value(face.face) : hash_value(face.base));
    return hash;
}

Window::Setup Window::build_setup(const Context& context) const
{
    return {m_position, m_dimensions,
            context.buffer().timestamp(),
            compute_faces_hash(context.faces()),
            context.selections().main_index(),
            context.selections() | gather<Vector<BasicSelection, MemoryDomain::Display>>()};
}

bool Window::needs_redraw(const Context& context) const
{
    auto& selections = context.selections();
    return m_position != m_last_setup.position or
        m_dimensions != m_last_setup.dimensions or
        context.buffer().timestamp() != m_last_setup.timestamp or
        selections.main_index() != m_last_setup.main_selection or
        selections.size() != m_last_setup.selections.size() or
        compute_faces_hash(context.faces()) != m_last_setup.faces_hash or
        not std::equal(selections.begin(), selections.end(),
                       m_last_setup.selections.begin(), m_last_setup.selections.end());
}

const DisplayBuffer& Window::update_display_buffer(const Context& context)
{
    ProfileScope profile{context, [&](std::chrono::microseconds duration) {
        write_to_debug_buffer(format("window display update for '{}' took {} us",
                                     buffer().display_name(), (size_t)duration.count()));
    }, not (buffer().flags() & Buffer::Flags::Debug)};

    if (m_display_buffer.timestamp() != -1)
    {
        for (auto&& change : buffer().changes_since(m_display_buffer.timestamp()))
        {
            if (change.type == Buffer::Change::Insert and change.begin.line < m_position.line)
                m_position.line += change.end.line - change.begin.line;
            if (change.type == Buffer::Change::Erase and change.begin.line < m_position.line)
                m_position.line = std::max(m_position.line - (change.end.line - change.begin.line), change.begin.line);
        }
    }

    DisplayLineList& lines = m_display_buffer.lines();
    m_display_buffer.set_timestamp(buffer().timestamp());
    lines.clear();

    if (m_dimensions == DisplayCoord{0,0})
        return m_display_buffer;

    kak_assert(&buffer() == &context.buffer());
    DisplaySetup setup = compute_display_setup(context);

    if (setup.line_count != m_last_display_setup.line_count or
        setup.widget_columns != m_last_display_setup.widget_columns)
    {
        // Technically the window has not resized, but most things that hook WinResize probably want to be notfied anyway
        m_resize_hook_pending = true;
    }

    for (LineCount line = 0; line < setup.line_count; ++line)
    {
        LineCount buffer_line = setup.first_line + line;
        if (buffer_line >= buffer().line_count())
            break;
        lines.emplace_back(AtomList{{buffer(), {buffer_line, {buffer_line, buffer()[buffer_line].length()}}}});
    }

    m_display_buffer.compute_range();
    const BufferRange range{{0,0}, buffer().end_coord()};
    for (auto pass : {HighlightPass::Replace, HighlightPass::Wrap, HighlightPass::Move})
        m_builtin_highlighters.highlight({context, setup, pass, {}}, m_display_buffer, range);

    if (context.ensure_cursor_visible)
    {
        auto cursor_pos = display_coord(context.selections().main().cursor());
        kak_assert(cursor_pos);

        if (auto line_overflow = cursor_pos->line - m_dimensions.line + setup.scroll_offset.line + 1; line_overflow > 0)
        {
            lines.erase(lines.begin(), lines.begin() + (size_t)line_overflow);
            setup.first_line = lines.begin()->range().begin.line;
        }

        auto max_first_column = cursor_pos->column - (setup.widget_columns + setup.scroll_offset.column);
        setup.first_column = std::min(setup.first_column, max_first_column);

        auto min_first_column = cursor_pos->column - (m_dimensions.column - setup.scroll_offset.column) + 1;
        setup.first_column = std::max(setup.first_column, min_first_column);
    }

    for (auto& line : m_display_buffer.lines())
        line.trim_from(setup.widget_columns, setup.first_column, m_dimensions.column);
    if (m_display_buffer.lines().size() > m_dimensions.line)
        m_display_buffer.lines().resize((size_t)m_dimensions.line);

    m_builtin_highlighters.highlight({context, setup, HighlightPass::Colorize, {}}, m_display_buffer, range);

    m_display_buffer.optimize();

    set_position({setup.first_line, setup.first_column});
    m_last_setup = build_setup(context);
    m_last_display_setup = setup;

    return m_display_buffer;
}

void Window::set_position(DisplayCoord position)
{
    m_position.line = clamp(position.line, 0_line, buffer().line_count()-1);
    m_position.column = std::max(0_col, position.column);
}

void Window::set_dimensions(DisplayCoord dimensions)
{
    if (m_dimensions != dimensions)
    {
        m_dimensions = dimensions;
        m_resize_hook_pending = true;
    }
}

void Window::run_resize_hook_ifn()
{
    if (m_resize_hook_pending)
    {
        m_resize_hook_pending = false;
        run_hook_in_own_context(Hook::WinResize,
                                format("{}.{}", m_dimensions.line, m_dimensions.column));
    }
}

static void check_display_setup(const DisplaySetup& setup, const Window& window)
{
    kak_assert(setup.first_line >= 0 and setup.first_line < window.buffer().line_count());
    kak_assert(setup.first_column >= 0);
    kak_assert(setup.line_count >= 0);
}

DisplaySetup Window::compute_display_setup(const Context& context) const
{
    DisplayCoord offset = options()["scrolloff"].get<DisplayCoord>();
    offset.line = std::min(offset.line, (m_dimensions.line + 1) / 2);
    offset.column = std::min(offset.column, (m_dimensions.column + 1) / 2);
    const auto& cursor = context.selections().main().cursor();
    DisplaySetup setup{m_position.line, m_dimensions.line, m_position.column, 0_col, offset};
    if (context.ensure_cursor_visible and
        cursor.line - offset.line < setup.first_line)
        setup.first_line = clamp(cursor.line - offset.line, 0_line, buffer().line_count()-1);

    for (auto pass : {HighlightPass::Move, HighlightPass::Wrap, HighlightPass::Replace})
        m_builtin_highlighters.compute_display_setup({context, setup, pass, {}}, setup);

    if (context.ensure_cursor_visible and
        cursor.line + offset.line >= setup.first_line + setup.line_count)
        setup.first_line = std::min(cursor.line + offset.line - setup.line_count + 1, buffer().line_count()-1);

    setup.first_line = std::min(setup.first_line, buffer().line_count()-1);
    check_display_setup(setup, *this);

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
    const auto& range = line.range();
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
    return range.end == BufferCoord{0,0} ?
        range.end : buffer.prev(range.end);
}
}

Optional<DisplayCoord> Window::display_coord(BufferCoord coord) const
{
    if (m_display_buffer.timestamp() != buffer().timestamp())
        return {};

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

Optional<BufferCoord> Window::buffer_coord(DisplayCoord coord) const
{
    if (m_display_buffer.timestamp() != buffer().timestamp() or
        m_display_buffer.lines().empty())
        return {};
    if (coord < 0_line)
        return {};

    auto line = std::min((size_t)coord.line, m_display_buffer.lines().size() - 1);
    return find_buffer_coord(m_display_buffer.lines()[line],
                             buffer(), coord.column);
}

void Window::clear_display_buffer()
{
    m_display_buffer = DisplayBuffer{};
}

void Window::on_option_changed(const Option& option)
{
    run_hook_in_own_context(Hook::WinSetOption, format("{}={}", option.name(), option.get_desc_string()));
}


void Window::run_hook_in_own_context(Hook hook, StringView param, String client_name)
{
    if (m_buffer->flags() & Buffer::Flags::NoHooks)
        return;

    InputHandler hook_handler{{ *m_buffer, Selection{} },
                              Context::Flags::Draft,
                              std::move(client_name)};
    hook_handler.context().set_window(*this);
    if (m_client)
        hook_handler.context().set_client(*m_client);

    hooks().run_hook(hook, param, hook_handler.context());
}
}
