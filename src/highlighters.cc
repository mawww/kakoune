#include "highlighters.hh"

#include "assert.hh"
#include "window.hh"
#include "display_buffer.hh"
#include "highlighter_registry.hh"
#include "highlighter_group.hh"
#include <boost/regex.hpp>

namespace Kakoune
{

void colorize_regex_range(DisplayBuffer& display_buffer,
                          const BufferIterator& range_begin,
                          const BufferIterator& range_end,
                          const boost::regex& ex,
                          Color fg_color, Color bg_color = Color::Default)
{
    assert(range_begin <= range_end);

    if (range_begin >= display_buffer.back().end() or
        range_end <= display_buffer.front().begin())
        return;

    BufferIterator display_begin = std::max(range_begin,
                                            display_buffer.front().begin());
    BufferIterator display_end   = std::min(range_end,
                                            display_buffer.back().end());

    boost::regex_iterator<BufferIterator> re_it(display_begin, display_end,
                                                ex, boost::match_nosubs);
    boost::regex_iterator<BufferIterator> re_end;
    DisplayBuffer::iterator atom_it = display_buffer.begin();
    for (; re_it != re_end; ++re_it)
    {
        BufferIterator begin = (*re_it)[0].first;
        BufferIterator end   = (*re_it)[0].second;
        assert(begin != end);

        auto begin_atom_it = display_buffer.atom_containing(begin, atom_it);
        assert(begin_atom_it != display_buffer.end());
        if (begin_atom_it->begin() != begin)
            begin_atom_it = ++display_buffer.split(begin_atom_it, begin);

        auto end_atom_it = display_buffer.atom_containing(end, begin_atom_it);
        if (end_atom_it != display_buffer.end() and
            end_atom_it->begin() != end)
            end_atom_it = ++display_buffer.split(end_atom_it, end);

        assert(begin_atom_it != end_atom_it);

        for (auto it = begin_atom_it; it != end_atom_it; ++it)
        {
            it->fg_color() = fg_color;
            it->bg_color() = bg_color;
        }

        atom_it = end_atom_it;
    }
}

void colorize_regex(DisplayBuffer& display_buffer,
                    const boost::regex& ex,
                    Color fg_color, Color bg_color = Color::Default)
{
    colorize_regex_range(display_buffer, display_buffer.front().begin(),
                         display_buffer.back().end(), ex, fg_color, bg_color);
}

Color parse_color(const std::string& color)
{
    if (color == "default") return Color::Default;
    if (color == "black")   return Color::Black;
    if (color == "red")     return Color::Red;
    if (color == "green")   return Color::Green;
    if (color == "yellow")  return Color::Yellow;
    if (color == "blue")    return Color::Blue;
    if (color == "magenta") return Color::Magenta;
    if (color == "cyan")    return Color::Cyan;
    if (color == "white")   return Color::White;
    return Color::Default;
}

HighlighterAndId colorize_regex_factory(Window& window,
                                        const HighlighterParameters params)
{
    if (params.size() != 3)
        throw runtime_error("wrong parameter count");

    boost::regex ex(params[0]);

    Color fg_color = parse_color(params[1]);
    Color bg_color = parse_color(params[2]);

    std::string id = "colre'" + params[0] + "'";

    return HighlighterAndId(id, std::bind(colorize_regex, std::placeholders::_1,
                                     ex, fg_color, bg_color));
}

void expand_tabulations(DisplayBuffer& display_buffer)
{
    const int tabstop = 8;
    for (auto atom_it = display_buffer.begin();
         atom_it != display_buffer.end(); ++atom_it)
    {
        for (BufferIterator it = atom_it->begin(); it != atom_it->end(); ++it)
        {
            if (*it == '\t')
            {
                if (it != atom_it->begin())
                    atom_it = ++display_buffer.split(atom_it, it);

                if (it+1 != atom_it->end())
                    atom_it = display_buffer.split(atom_it, it+1);

                BufferCoord pos = it.buffer().line_and_column_at(it);

                int column = 0;
                for (auto line_it = it.buffer().iterator_at({pos.line, 0});
                     line_it != it; ++line_it)
                {
                    assert(*line_it != '\n');
                    if (*line_it == '\t')
                        column += tabstop - (column % tabstop);
                    else
                       ++column;
                }

                int count = tabstop - (column % tabstop);
                display_buffer.replace_atom_content(atom_it,
                                                    std::string(count, ' '));
            }
        }
    }
}

void show_line_numbers(DisplayBuffer& display_buffer)
{
    const Buffer& buffer = display_buffer.front().begin().buffer();
    BufferCoord coord = buffer.line_and_column_at(display_buffer.begin()->begin());

    int last_line = buffer.line_count()-1;
    int digit_count = 0;
    for (int c = last_line; c > 0; c /= 10)
        ++digit_count;

    char format[] = "%?d ";
    format[1] = '0' + digit_count;

    for (; coord.line <= last_line; ++coord.line)
    {
        BufferIterator line_start = buffer.iterator_at(coord);
        DisplayBuffer::iterator atom_it = display_buffer.atom_containing(line_start);
        if (atom_it != display_buffer.end())
        {
            if (atom_it->begin() != line_start)
            {
                if (not atom_it->splitable())
                    continue;

                atom_it = ++display_buffer.split(atom_it, line_start);
            }
            atom_it = display_buffer.insert(
                atom_it,
                DisplayAtom(atom_it->coord(),
                            atom_it->begin(), atom_it->begin(),
                            Color::Black, Color::White));

            char buffer[10];
            snprintf(buffer, 10, format, coord.line + 1);
            display_buffer.replace_atom_content(atom_it, buffer);
        }
    }
}

template<void (*highlighter_func)(DisplayBuffer&)>
class SimpleHighlighterFactory
{
public:
    SimpleHighlighterFactory(const std::string& id) : m_id(id) {}

    HighlighterAndId operator()(Window& window,
                                const HighlighterParameters& params) const
    {
        return HighlighterAndId(m_id, HighlighterFunc(highlighter_func));
    }
private:
    std::string m_id;
};

class SelectionsHighlighter
{
public:
    SelectionsHighlighter(Window& window)
        : m_window(window)
    {
    }

    void operator()(DisplayBuffer& display_buffer)
    {
        typedef std::pair<BufferIterator, BufferIterator> BufferRange;

        std::vector<BufferRange> selections;
        for (auto& sel : m_window.selections())
            selections.push_back(BufferRange(sel.begin(), sel.end()));

        std::sort(selections.begin(), selections.end(),
                  [](const BufferRange& lhs, const BufferRange& rhs)
                  { return lhs.first < rhs.first; });

        auto atom_it = display_buffer.begin();
        auto sel_it = selections.begin();

        // underline each selections
        while (atom_it != display_buffer.end()
               and sel_it != selections.end())
        {
            BufferRange& sel = *sel_it;
            DisplayAtom& atom = *atom_it;

            // [###------]
            if (atom.begin() >= sel.first and atom.begin() < sel.second and
                atom.end() > sel.second)
            {
                atom_it = display_buffer.split(atom_it, sel.second);
                atom_it->attribute() |= Attributes::Underline;
                ++atom_it;
                ++sel_it;
            }
            // [---###---]
            else if (atom.begin() < sel.first and atom.end() > sel.second)
            {
                atom_it = display_buffer.split(atom_it, sel.first);
                atom_it = display_buffer.split(++atom_it, sel.second);
                atom_it->attribute() |= Attributes::Underline;
                ++atom_it;
                ++sel_it;
            }
            // [------###]
            else if (atom.begin() < sel.first and atom.end() > sel.first)
            {
                atom_it = ++display_buffer.split(atom_it, sel.first);
                atom_it->attribute() |= Attributes::Underline;
                ++atom_it;
            }
            // [#########]
            else if (atom.begin() >= sel.first and atom.end() <= sel.second)
            {
                atom_it->attribute() |= Attributes::Underline;
                ++atom_it;
            }
            // [---------]
            else if (atom.begin() >= sel.second)
                ++sel_it;
            // [---------]
            else if (atom.end() <= sel.first)
                ++atom_it;
            else
                assert(false);
        }

        // invert selection last char
        for (auto& sel : m_window.selections())
        {
            const BufferIterator& last = sel.last();

            DisplayBuffer::iterator atom_it = display_buffer.atom_containing(last);
            if (atom_it == display_buffer.end())
                continue;

            if (atom_it->begin() < last)
                atom_it = ++display_buffer.split(atom_it, last);
            if (atom_it->end() > last + 1)
                atom_it = display_buffer.split(atom_it, last + 1);

            atom_it->attribute() |= Attributes::Reverse;
        }

    }

    static HighlighterAndId create(Window& window,
                                   const HighlighterParameters& params)
    {
        return HighlighterAndId("highlight_selections",
                            SelectionsHighlighter(window));
    }

private:
    const Window& m_window;
};

HighlighterAndId highlighter_group_factory(Window& window,
                                           const HighlighterParameters& params)
{
    if (params.size() != 1)
        throw runtime_error("wrong parameter count");

    return HighlighterAndId(params[0], HighlighterGroup());
}

void register_highlighters()
{
    HighlighterRegistry& registry = HighlighterRegistry::instance();

    registry.register_factory("highlight_selections", SelectionsHighlighter::create);
    registry.register_factory("expand_tabs", SimpleHighlighterFactory<expand_tabulations>("expand_tabs"));
    registry.register_factory("number_lines", SimpleHighlighterFactory<show_line_numbers>("number_lines"));
    registry.register_factory("regex", colorize_regex_factory);
    registry.register_factory("group", highlighter_group_factory);
}

}
