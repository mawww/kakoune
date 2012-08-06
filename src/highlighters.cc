#include "highlighters.hh"

#include "assert.hh"
#include "window.hh"
#include "highlighter_registry.hh"
#include "highlighter_group.hh"
#include "regex.hh"

namespace Kakoune
{

using namespace std::placeholders;

typedef boost::regex_iterator<BufferIterator> RegexIterator;

template<typename T>
void highlight_range(DisplayBuffer& display_buffer,
                     BufferIterator begin, BufferIterator end,
                     bool skip_replaced, T func)
{
    if (end <= display_buffer.range().first or begin >= display_buffer.range().second)
        return;

    for (auto& line : display_buffer.lines())
    {
        if (line.buffer_line() >= begin.line() and line.buffer_line() <= end.line())
        {
            for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
            {
                bool is_replaced = atom_it->content.type() == AtomContent::ReplacedBufferRange;

                if (not atom_it->content.has_buffer_range() or
                    (skip_replaced and is_replaced))
                    continue;

                if (end <= atom_it->content.begin() or begin >= atom_it->content.end())
                    continue;

                if (not is_replaced and begin > atom_it->content.begin())
                    atom_it = ++line.split(atom_it, begin);

                if (not is_replaced and end < atom_it->content.end())
                {
                    atom_it = line.split(atom_it, end);
                    func(*atom_it);
                    ++atom_it;
                }
                else
                    func(*atom_it);
            }
        }
    }
}

typedef std::unordered_map<size_t, std::pair<Color, Color>> ColorSpec;
void colorize_regex(DisplayBuffer& display_buffer,
                    const Regex& ex,
                    ColorSpec colors)
{
    const BufferRange& range = display_buffer.range();
    RegexIterator re_it(range.first, range.second, ex);
    RegexIterator re_end;
    for (; re_it != re_end; ++re_it)
    {
        for (size_t n = 0; n < re_it->size(); ++n)
        {
            if (colors.find(n) == colors.end())
                continue;

            highlight_range(display_buffer, (*re_it)[n].first, (*re_it)[n].second, true,
                            [&](DisplayAtom& atom) {
                                atom.fg_color = colors[n].first;
                                atom.bg_color = colors[n].second;
                            });
        }
    }
}

Color parse_color(const String& color)
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
    if (params.size() < 2)
        throw runtime_error("wrong parameter count");

    Regex ex(params[0].begin(), params[0].end(),
             boost::regex::perl | boost::regex::optimize);

    static Regex color_spec_ex(LR"((\d+):(\w+)(,(\w+))?)");
    ColorSpec colors;
    for (auto it = params.begin() + 1;  it != params.end(); ++it)
    {
        boost::match_results<String::iterator> res;
        if (not boost::regex_match(it->begin(), it->end(), res, color_spec_ex))
            throw runtime_error("wrong colorspec: '" + *it +
                                 "' expected <capture>:<fgcolor>[,<bgcolor>]");

        int capture = str_to_int(String(res[1].first, res[1].second));
        Color fg_color = parse_color(String(res[2].first, res[2].second));
        Color bg_color = res[4].matched ?
                           parse_color(String(res[4].first, res[4].second))
                         : Color::Default;
        colors[capture] = { fg_color, bg_color };
    }

    String id = "colre'" + params[0] + "'";

    return HighlighterAndId(id, std::bind(colorize_regex, _1,  ex,
                                          std::move(colors)));
}

void expand_tabulations(Window& window, DisplayBuffer& display_buffer)
{
    const int tabstop = window.option_manager()["tabstop"].as_int();
    for (auto& line : display_buffer.lines())
    {
        for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
        {
            if (atom_it->content.type() != AtomContent::BufferRange)
                continue;

            auto begin = atom_it->content.begin();
            auto end = atom_it->content.end();
            for (BufferIterator it = begin; it != end; ++it)
            {
                if (*it == '\t')
                {
                    if (it != begin)
                        atom_it = ++line.split(atom_it, it);
                    if (it+1 != end)
                        atom_it = line.split(atom_it, it+1);

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
                    String padding;
                    for (int i = 0; i < count; ++i)
                        padding += ' ';
                    atom_it->content.replace(padding);
                    break;
                }
            }
        }
    }
}

void show_line_numbers(Window& window, DisplayBuffer& display_buffer)
{
    int last_line = window.buffer().line_count();
    int digit_count = 0;
    for (int c = last_line; c > 0; c /= 10)
        ++digit_count;

    char format[] = "%?d ";
    format[1] = '0' + digit_count;

    for (auto& line : display_buffer.lines())
    {
        char buffer[10];
        snprintf(buffer, 10, format, line.buffer_line() + 1);
        DisplayAtom atom = DisplayAtom(AtomContent(buffer));
        atom.fg_color = Color::Black;
        atom.bg_color = Color::White;
        line.insert(line.begin(), std::move(atom));
    }
}

void highlight_selections(Window& window, DisplayBuffer& display_buffer)
{
    for (auto& sel : window.selections())
    {
        highlight_range(display_buffer, sel.begin(), sel.end(), false,
                        [](DisplayAtom& atom) { atom.attribute |= Attributes::Underline; });

        const BufferIterator& last = sel.last();
        highlight_range(display_buffer, last, last+1, false,
                        [](DisplayAtom& atom) { atom.attribute |= Attributes::Reverse; });
    }
}

template<void (*highlighter_func)(DisplayBuffer&)>
class SimpleHighlighterFactory
{
public:
    SimpleHighlighterFactory(const String& id) : m_id(id) {}

    HighlighterAndId operator()(Window& window,
                                const HighlighterParameters& params) const
    {
        return HighlighterAndId(m_id, HighlighterFunc(highlighter_func));
    }
private:
    String m_id;
};

template<void (*highlighter_func)(Window&, DisplayBuffer&)>
class WindowHighlighterFactory
{
public:
    WindowHighlighterFactory(const String& id) : m_id(id) {}

    HighlighterAndId operator()(Window& window,
                                const HighlighterParameters& params) const
    {
        return HighlighterAndId(m_id, std::bind(highlighter_func, std::ref(window), _1));
    }
private:
    String m_id;
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

    registry.register_factory("highlight_selections", WindowHighlighterFactory<highlight_selections>("highlight_selections"));
    registry.register_factory("expand_tabs", WindowHighlighterFactory<expand_tabulations>("expand_tabs"));
    registry.register_factory("number_lines", WindowHighlighterFactory<show_line_numbers>("number_lines"));
    registry.register_factory("regex", colorize_regex_factory);
    registry.register_factory("group", highlighter_group_factory);
}

}
