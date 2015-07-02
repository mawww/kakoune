#include "window.hh"

#include "assert.hh"
#include "context.hh"
#include "highlighter.hh"
#include "hook_manager.hh"
#include "input_handler.hh"
#include "user_interface.hh"

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

size_t Window::compute_hash(const Context& context) const
{
    size_t res = hash_values(m_position, context.ui().dimensions(), context.buffer().timestamp());

    auto& selections = context.selections();
    res = combine_hash(res, hash_value(selections.main_index()));
    for (auto& sel : selections)
        res = combine_hash(res, hash_values((const ByteCoord&)sel.cursor(), sel.anchor()));

    return res;
}

bool Window::needs_redraw(const Context& context) const
{
    size_t hash = compute_hash(context);
    return hash != m_hash;
}

const DisplayBuffer& Window::update_display_buffer(const Context& context)
{
    DisplayBuffer::LineList& lines = m_display_buffer.lines();
    lines.clear();

    m_dimensions = context.ui().dimensions();
    if (m_dimensions == CharCoord{0,0})
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

    m_hash = compute_hash(context);

    return m_display_buffer;
}

void Window::set_position(CharCoord position)
{
    m_position.line = std::max(0_line, position.line);
    m_position.column = std::max(0_char, position.column);
}

void Window::set_dimensions(CharCoord dimensions)
{
    m_dimensions = dimensions;
}

static LineCount adapt_view_pos(LineCount line, LineCount offset,
                                LineCount view_pos, LineCount view_size,
                                LineCount buffer_size, bool scrolloffalways)
{
    if (line - offset < view_pos)
        return std::max(0_line, line - offset);
    else if (line + offset >= view_pos + view_size)
    {
        if (scrolloffalways)
            return std::max(0_line, line + offset - (view_size - 1));
        else
            return std::min(std::max(0_line, buffer_size - view_size),
                            line + offset - (view_size - 1));
    }
    return view_pos;
}

static CharCount adapt_view_pos(const DisplayBuffer& display_buffer, CharCount offset,
                                ByteCoord pos, CharCount view_pos, CharCount view_size)
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

                    if (pos_beg - offset < view_pos)
                        return std::max(0_char, pos_beg - offset);

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

    const CharCoord max_offset{(m_dimensions.line - 1)/2,
                               (m_dimensions.column - 1)/2};
    const CharCoord offset = std::min(options()["scrolloff"].get<CharCoord>(),
                                      max_offset);
    const bool scrolloffalways = options()["scrolloffalways"].get<bool>();

    // scroll lines if needed, try to get as much of the selection visible as possible
    m_position.line = adapt_view_pos(anchor.line, offset.line, m_position.line,
                                     m_dimensions.line, buffer().line_count(),
                                     scrolloffalways);
    m_position.line = adapt_view_pos(cursor.line,  offset.line, m_position.line,
                                     m_dimensions.line, buffer().line_count(),
                                     scrolloffalways);

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
CharCount find_display_column(const DisplayLine& line, const Buffer& buffer,
                              ByteCoord coord)
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

ByteCoord find_buffer_coord(const DisplayLine& line, const Buffer& buffer,
                            CharCount column)
{
    auto& range = line.range();
    for (auto& atom : line)
    {
        CharCount len = atom.length();
        if (atom.has_buffer_range() and column < len)
        {
            if (atom.type() == DisplayAtom::BufferRange)
                return utf8::advance(buffer.iterator_at(atom.begin()), buffer.iterator_at(range.end),
                                     std::max(0_char, column)).coord();
             return atom.begin();
        }
        column -= len;
    }
    return buffer.clamp(buffer.prev(range.end));
}
}

CharCoord Window::display_position(ByteCoord coord) const
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

ByteCoord Window::buffer_coord(CharCoord coord) const
{
    if (coord <= 0_line)
        coord = {0,0};
    if ((int)coord.line >= m_display_buffer.lines().size())
        coord = CharCoord{(int)m_display_buffer.lines().size()-1, INT_MAX};

    return find_buffer_coord(m_display_buffer.lines()[(int)coord.line],
                             buffer(), coord.column);
}

ByteCoord Window::offset_coord(ByteCoord coord, CharCount offset)
{
    return buffer().offset_coord(coord, offset);
}

ByteCoordAndTarget Window::offset_coord(ByteCoordAndTarget coord, LineCount offset)
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

    CharCount column = coord.target == -1 ? find_display_column(lines[0], buffer(), coord) : coord.target;
    return { find_buffer_coord(lines[1], buffer(), column), column };
}

void Window::clear_display_buffer()
{
    m_display_buffer = DisplayBuffer{};
}

void Window::on_option_changed(const Option& option)
{
    run_hook_in_own_context("WinSetOption",
                            format("{}={}", option.name(), option.get_as_string()));

    // an highlighter might depend on the option, so we need to redraw
    force_redraw();
}


void Window::run_hook_in_own_context(StringView hook_name, StringView param)
{
    InputHandler hook_handler({ *m_buffer, Selection{} }, Context::Flags::Transient);
    hook_handler.context().set_window(*this);
    hooks().run_hook(hook_name, param, hook_handler.context());
}
}
