#include "filters.hh"

#include "assert.hh"
#include "window.hh"
#include "display_buffer.hh"
#include "filter_registry.hh"
#include <boost/regex.hpp>

namespace Kakoune
{

void colorize_regex_range(DisplayBuffer& display_buffer,
                          const BufferIterator& range_begin,
                          const BufferIterator& range_end,
                          const boost::regex& ex,
                          Color fg_color, Color bg_color = Color::Default)
{
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

FilterAndId colorize_regex_factory(Window& window,
                                   const FilterParameters params)
{
    if (params.size() != 3)
        throw runtime_error("wrong parameter count");

    boost::regex ex(params[0]);

    Color fg_color = parse_color(params[1]);
    Color bg_color = parse_color(params[2]);

    std::string id = "colre'" + params[0] + "'";

    return FilterAndId(id, std::bind(colorize_regex, std::placeholders::_1,
                                     ex, fg_color, bg_color));
}

void colorize_cplusplus(DisplayBuffer& display_buffer)
{
    static boost::regex preprocessor("(\\`|(?<=\\n))\\h*#\\h*[^\\n]*(?=\\n)");
    colorize_regex(display_buffer, preprocessor, Color::Magenta);

    static boost::regex comments("//[^\\n]*\\n");
    colorize_regex(display_buffer, comments, Color::Cyan);

    static boost::regex strings("(?<!')\"(\\\\\"|[^\"])*\"");
    colorize_regex(display_buffer, strings, Color::Magenta);

    static boost::regex values("\\<(true|false|NULL|nullptr)\\>|\\<-?\\d+[fdiu]?|'\\\\?[^']?'");
    colorize_regex(display_buffer, values, Color::Red);

    static boost::regex builtin_types("\\<(void|int|char|unsigned|float|bool|size_t)\\>");
    colorize_regex(display_buffer, builtin_types, Color::Yellow);

    static boost::regex control_keywords("\\<(while|for|if|else|do|switch|case|default|goto|break|continue|return|using|try|catch|throw)\\>");
    colorize_regex(display_buffer, control_keywords, Color::Blue);

    //static boost::regex operators("->|\\+|\\-|\\*|/|\\\\|\\&|\\|\\^|[<>=!+-]=|=|\\(|\\)|\\[|\\]|\\{|\\}|\\<(not|and|or|xor)\\>");
    //colorize_regex(display_buffer, operators, Color::Green);

    static boost::regex types_keywords("\\<(const|auto|namespace|static|volatile|class|struct|enum|union|public|protected|private|template|typedef|virtual)\\>");
    colorize_regex(display_buffer, types_keywords, Color::Green);
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

    int last_line = buffer.line_and_column_at(display_buffer.back().end()-1).line;

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

            char buffer[6];
            snprintf(buffer, 6, "%3d ", coord.line + 1);
            display_buffer.replace_atom_content(atom_it, buffer);
        }
    }
}

template<void (*filter_func)(DisplayBuffer&)>
class SimpleFilterFactory
{
public:
    SimpleFilterFactory(const std::string& id) : m_id(id) {}

    FilterAndId operator()(Window& window,
                           const FilterParameters& params) const
    {
        return FilterAndId(m_id, FilterFunc(filter_func));
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
        SelectionList sorted_selections = m_window.selections();

        std::sort(sorted_selections.begin(), sorted_selections.end(),
                  [](const Selection& lhs, const Selection& rhs) { return lhs.begin() < rhs.begin(); });

        auto atom_it = display_buffer.begin();
        auto sel_it = sorted_selections.begin();

        while (atom_it != display_buffer.end()
               and sel_it != sorted_selections.end())
        {
            Selection& sel = *sel_it;
            DisplayAtom& atom = *atom_it;

            // [###------]
            if (atom.begin() >= sel.begin() and atom.begin() < sel.end() and atom.end() > sel.end())
            {
                atom_it = display_buffer.split(atom_it, sel.end());
                atom_it->attribute() |= Attributes::Underline;
                ++atom_it;
                ++sel_it;
            }
            // [---###---]
            else if (atom.begin() < sel.begin() and atom.end() > sel.end())
            {
                atom_it = display_buffer.split(atom_it, sel.begin());
                atom_it = display_buffer.split(++atom_it, sel.end());
                atom_it->attribute() |= Attributes::Underline;
                ++atom_it;
                ++sel_it;
            }
            // [------###]
            else if (atom.begin() < sel.begin() and atom.end() > sel.begin())
            {
                atom_it = ++display_buffer.split(atom_it, sel.begin());
                atom_it->attribute() |= Attributes::Underline;
                ++atom_it;
            }
            // [#########]
            else if (atom.begin() >= sel.begin() and atom.end() <= sel.end())
            {
                atom_it->attribute() |= Attributes::Underline;
                ++atom_it;
            }
            // [---------]
            else if (atom.begin() >= sel.end())
                ++sel_it;
            // [---------]
            else if (atom.end() <= sel.begin())
                ++atom_it;
            else
                assert(false);
        }
    }

    static FilterAndId create(Window& window,
                              const FilterParameters& params)
    {
        return FilterAndId("highlight_selections",
                            SelectionsHighlighter(window));
    }

private:
    const Window& m_window;
};

void register_filters()
{
    FilterRegistry& registry = FilterRegistry::instance();

    registry.register_factory("highlight_selections", SelectionsHighlighter::create);
    registry.register_factory("expand_tabs", SimpleFilterFactory<expand_tabulations>("expand_tabs"));
    registry.register_factory("number_lines", SimpleFilterFactory<show_line_numbers>("number_lines"));
    registry.register_factory("hlcpp", SimpleFilterFactory<colorize_cplusplus>("hlcpp"));
    registry.register_factory("regex", colorize_regex_factory);
}

}
