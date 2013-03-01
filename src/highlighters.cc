#include "highlighters.hh"
#include "assert.hh"
#include "color_registry.hh"
#include "highlighter_group.hh"
#include "register_manager.hh"
#include "context.hh"
#include "string.hh"
#include "utf8.hh"
#include "utf8_iterator.hh"

#include <sstream>
#include <locale>

namespace Kakoune
{

using namespace std::placeholders;

typedef boost::regex_iterator<BufferIterator> RegexIterator;

template<typename T>
void highlight_range(DisplayBuffer& display_buffer,
                     BufferIterator begin, BufferIterator end,
                     bool skip_replaced, T func)
{
    if (begin == end or end <= display_buffer.range().first
                     or begin >= display_buffer.range().second)
        return;

    for (auto& line : display_buffer.lines())
    {
        if (line.buffer_line() < begin.line() or  end.line() < line.buffer_line())
            continue;

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

typedef std::unordered_map<size_t, const ColorPair*> ColorSpec;

class RegexColorizer
{
public:
    RegexColorizer(Regex regex, ColorSpec colors)
        : m_regex(std::move(regex)), m_colors(std::move(colors)),
          m_cache_timestamp(0)
    {
    }

    void operator()(DisplayBuffer& display_buffer)
    {
        update_cache_ifn(display_buffer.range());
        for (auto& match : m_cache_matches)
        {
            for (size_t n = 0; n < match.size(); ++n)
            {
                auto col_it = m_colors.find(n);
                if (col_it == m_colors.end())
                    continue;

                highlight_range(display_buffer, match[n].first, match[n].second, true,
                                [&](DisplayAtom& atom) {
                                    atom.fg_color = col_it->second->first;
                                    atom.bg_color = col_it->second->second;
                                });
            }
        }
    }

private:
    BufferRange m_cache_range;
    size_t      m_cache_timestamp;
    std::vector<boost::match_results<BufferIterator>> m_cache_matches;

    Regex     m_regex;
    ColorSpec m_colors;

    void update_cache_ifn(const BufferRange& range)
    {
        const Buffer& buf = range.first.buffer();
        if (m_cache_range.first.is_valid() and
            &m_cache_range.first.buffer() == &buf and
            buf.timestamp() == m_cache_timestamp and
            range.first >= m_cache_range.first and
            range.second <= m_cache_range.second)
           return;

        m_cache_matches.clear();
        m_cache_range.first  = buf.iterator_at_line_begin(range.first.line() - 10);
        m_cache_range.second = buf.iterator_at_line_end(range.second.line() + 10);
        m_cache_timestamp = buf.timestamp();

        RegexIterator re_it(m_cache_range.first, m_cache_range.second, m_regex);
        RegexIterator re_end;
        for (; re_it != re_end; ++re_it)
            m_cache_matches.push_back(*re_it);
    }
};

HighlighterAndId colorize_regex_factory(const HighlighterParameters params)
{
    if (params.size() < 2)
        throw runtime_error("wrong parameter count");

    try
    {
        static Regex color_spec_ex(R"((\d+):(\w+(,\w+)?))");
        ColorSpec colors;
        for (auto it = params.begin() + 1;  it != params.end(); ++it)
        {
            boost::match_results<String::iterator> res;
            if (not boost::regex_match(it->begin(), it->end(), res, color_spec_ex))
                throw runtime_error("wrong colorspec: '" + *it +
                                     "' expected <capture>:<fgcolor>[,<bgcolor>]");

            int capture = str_to_int(String(res[1].first, res[1].second));
            const ColorPair*& color = colors[capture];
            color = &ColorRegistry::instance()[String(res[2].first, res[2].second)];
        }

        String id = "colre'" + params[0] + "'";

        Regex ex(params[0].begin(), params[0].end(),
                 boost::regex::perl | boost::regex::optimize);

        return HighlighterAndId(id, RegexColorizer(std::move(ex),
                                                   std::move(colors)));
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error(String("regex error: ") + err.what());
    }
}

class SearchHighlighter
{
public:
    SearchHighlighter(const ColorSpec& colors)
        : m_colors(colors), m_colorizer(Regex(), m_colors) {}

    void operator()(DisplayBuffer& display_buffer)
    {
        memoryview<String> searches = RegisterManager::instance()['/'].values(Context{});
        if (searches.empty())
            return;
        const String& search = searches[0];
        if (search != m_last_search)
        {
            m_last_search = search;
            if (not m_last_search.empty())
                m_colorizer = RegexColorizer{Regex{m_last_search.begin(), m_last_search.end()}, m_colors};
        }
        if (not m_last_search.empty())
            m_colorizer(display_buffer);
    }

private:
    String         m_last_search;
    ColorSpec      m_colors;
    RegexColorizer m_colorizer;
};

HighlighterAndId highlight_search_factory(const HighlighterParameters params)
{
    if (params.size() != 1)
        throw runtime_error("wrong parameter count");
    try
    {
        ColorSpec colors;
        colors[0] = &ColorRegistry::instance()[params[0]];
        return {"hlsearch", SearchHighlighter{colors}};
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error(String("regex error: ") + err.what());
    }
};

void expand_tabulations(const OptionManager& options, DisplayBuffer& display_buffer)
{
    const int tabstop = options["tabstop"].as_int();
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

                    int column = 0;
                    for (auto line_it = it.buffer().iterator_at_line_begin(it);
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

void show_line_numbers(DisplayBuffer& display_buffer)
{
    LineCount last_line = display_buffer.range().first.buffer().line_count();
    int digit_count = 0;
    for (LineCount c = last_line; c > 0; c /= 10)
        ++digit_count;

    char format[] = "%?d ";
    format[1] = '0' + digit_count;

    for (auto& line : display_buffer.lines())
    {
        char buffer[10];
        snprintf(buffer, 10, format, (int)line.buffer_line() + 1);
        DisplayAtom atom = DisplayAtom(AtomContent(buffer));
        atom.fg_color = Color::Black;
        atom.bg_color = Color::White;
        line.insert(line.begin(), std::move(atom));
    }
}

void highlight_selections(const SelectionList& selections, DisplayBuffer& display_buffer)
{
    for (auto& sel : selections)
    {
        highlight_range(display_buffer, sel.begin(), sel.end(), false,
                        [](DisplayAtom& atom) { atom.attribute |= Attributes::Underline; });

        const BufferIterator& last = sel.last();
        highlight_range(display_buffer, last, utf8::next(last), false,
                        [](DisplayAtom& atom) { atom.attribute |= Attributes::Reverse;
                                                atom.attribute &= ~Attributes::Underline; });
    }
    const Selection& back = selections.back();
    highlight_range(display_buffer, back.begin(), back.end(), false,
                    [](DisplayAtom& atom) { atom.attribute |= Attributes::Bold; });
}

void expand_unprintable(DisplayBuffer& display_buffer)
{
    for (auto& line : display_buffer.lines())
    {
        for (auto& atom : line)
        {
            if (atom.content.type() == AtomContent::BufferRange)
            {
                using Utf8It = utf8::utf8_iterator<BufferIterator>;
                for (Utf8It it = atom.content.begin(), end = atom.content.end(); it != end; ++it)
                {
                    Codepoint cp = *it;
                    if (cp != '\n' and not std::isprint((wchar_t)cp, std::locale()))
                    {
                        std::ostringstream oss;
                        oss << "U+" << std::hex << cp;
                        String str = oss.str();
                        highlight_range(display_buffer,
                                        it.underlying_iterator(), (it+1).underlying_iterator(),
                                        true, [&str](DisplayAtom& atom){ atom.content.replace(str);
                                                                         atom.bg_color = Color::Red;
                                                                         atom.fg_color = Color::Black; });
                    }
                }
            }
        }
    }
}

template<void (*highlighter_func)(DisplayBuffer&)>
class SimpleHighlighterFactory
{
public:
    SimpleHighlighterFactory(const String& id) : m_id(id) {}

    HighlighterAndId operator()(const HighlighterParameters& params) const
    {
        return HighlighterAndId(m_id, HighlighterFunc(highlighter_func));
    }
private:
    String m_id;
};

HighlighterAndId highlighter_group_factory(const HighlighterParameters& params)
{
    if (params.size() != 1)
        throw runtime_error("wrong parameter count");

    return HighlighterAndId(params[0], HighlighterGroup());
}

void register_highlighters()
{
    HighlighterRegistry& registry = HighlighterRegistry::instance();

    registry.register_func("number_lines", SimpleHighlighterFactory<show_line_numbers>("number_lines"));
    registry.register_func("regex", colorize_regex_factory);
    registry.register_func("search", highlight_search_factory);
    registry.register_func("group", highlighter_group_factory);
}

}
