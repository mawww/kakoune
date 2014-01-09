#include "highlighters.hh"

#include "assert.hh"
#include "color_registry.hh"
#include "context.hh"
#include "display_buffer.hh"
#include "option_types.hh"
#include "register_manager.hh"
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
                     BufferCoord begin, BufferCoord end,
                     bool skip_replaced, T func)
{
    if (begin == end or end <= display_buffer.range().first
                     or begin >= display_buffer.range().second)
        return;

    for (auto& line : display_buffer.lines())
    {
        auto& range = line.range();
        if (range.second <= begin or  end < range.first)
            continue;

        for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
        {
            bool is_replaced = atom_it->type() == DisplayAtom::ReplacedBufferRange;

            if (not atom_it->has_buffer_range() or
                (skip_replaced and is_replaced))
                continue;

            if (end <= atom_it->begin() or begin >= atom_it->end())
                continue;

            if (not is_replaced and begin > atom_it->begin())
                atom_it = ++line.split(atom_it, begin);

            if (not is_replaced and end < atom_it->end())
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

template<typename T>
void apply_highlighter(const Context& context,
                       DisplayBuffer& display_buffer,
                       BufferCoord begin, BufferCoord end,
                       T&& highlighter)
{
    using LineIterator = DisplayBuffer::LineList::iterator;
    LineIterator first_line;
    std::vector<DisplayLine::iterator> insert_pos;
    auto line_end = display_buffer.lines().end();

    DisplayBuffer region_display;
    auto& region_lines = region_display.lines();
    for (auto line_it = display_buffer.lines().begin(); line_it != line_end; ++line_it)
    {
        auto& line = *line_it;
        auto& range = line.range();
        if (range.second <= begin or  end < range.first)
            continue;

        if (region_lines.empty())
            first_line = line_it;
        region_lines.emplace_back();
        insert_pos.emplace_back();

        if (range.first < begin or range.second > end)
        {
            size_t beg_idx = 0;
            size_t end_idx = line.atoms().size();

            for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
            {
                if (not atom_it->has_buffer_range() or end <= atom_it->begin() or begin >= atom_it->end())
                    continue;

                bool is_replaced = atom_it->type() == DisplayAtom::ReplacedBufferRange;
                if (atom_it->begin() <= begin)
                {
                    if (is_replaced or atom_it->begin() == begin)
                        beg_idx = atom_it - line.begin();
                    else
                    {
                        atom_it = ++line.split(atom_it, begin);
                        beg_idx = atom_it - line.begin();
                        ++end_idx;
                    }
                }

                if (atom_it->end() >= end)
                {
                    if (is_replaced or atom_it->end() == end)
                        beg_idx = atom_it - line.begin();
                    else
                    {
                        atom_it = ++line.split(atom_it, end);
                        end_idx = atom_it - line.begin();
                    }
                }
            }
            std::move(line.begin() + beg_idx, line.begin() + end_idx,
                      std::back_inserter(region_lines.back()));
            insert_pos.back() = line.erase(line.begin() + beg_idx, line.begin() + end_idx);
        }
        else
        {
            region_lines.back() = std::move(line);
            insert_pos.back() = line.begin();
        }
    }

    region_display.compute_range();
    highlighter(context, region_display);

    for (size_t i = 0; i < region_lines.size(); ++i)
    {
        auto& line = *(first_line + i);
        auto pos = insert_pos[i];
        for (auto& atom : region_lines[i])
            pos = ++line.insert(pos, std::move(atom));
    }
    display_buffer.compute_range();
}

typedef std::unordered_map<size_t, const ColorPair*> ColorSpec;

class RegexColorizer
{
public:
    RegexColorizer(Regex regex, ColorSpec colors)
        : m_regex{std::move(regex)}, m_colors{std::move(colors)},
          m_cache_id{ValueId::get_free_id()}
    {
    }

    void operator()(const Context& context, DisplayBuffer& display_buffer)
    {
        auto& cache = update_cache_ifn(context.buffer(), display_buffer.range());
        for (auto& match : cache.m_matches)
        {
            for (size_t n = 0; n < match.size(); ++n)
            {
                auto col_it = m_colors.find(n);
                if (col_it == m_colors.end())
                    continue;

                highlight_range(display_buffer, match[n].first, match[n].second, true,
                                [&](DisplayAtom& atom) { atom.colors = *col_it->second; });
            }
        }
    }

private:
    struct MatchesCache
    {
        BufferRange m_range;
        size_t      m_timestamp = 0;
        std::vector<std::vector<std::pair<BufferCoord, BufferCoord>>> m_matches;
    };

    Regex     m_regex;
    ColorSpec m_colors;
    ValueId   m_cache_id;

    MatchesCache& update_cache_ifn(const Buffer& buffer, const BufferRange& range)
    {
        Value& cache_val = buffer.values()[m_cache_id];
        if (not cache_val)
            cache_val = Value(MatchesCache{});
        MatchesCache& cache = cache_val.as<MatchesCache>();

        if (buffer.timestamp() == cache.m_timestamp and
            range.first >= cache.m_range.first and
            range.second <= cache.m_range.second)
           return cache;

        cache.m_range.first  = buffer.clamp({range.first.line - 10, 0});
        cache.m_range.second = buffer.next(buffer.clamp({range.second.line + 10, INT_MAX}));
        cache.m_timestamp = buffer.timestamp();

        cache.m_matches.clear();
        RegexIterator re_it{buffer.iterator_at(cache.m_range.first),
                            buffer.iterator_at(cache.m_range.second), m_regex};
        RegexIterator re_end;
        for (; re_it != re_end; ++re_it)
        {
            cache.m_matches.emplace_back();
            auto& match = cache.m_matches.back();
            for (auto& sub : *re_it)
                match.emplace_back(sub.first.coord(), sub.second.coord());
        }
        return cache;
    }
};

HighlighterAndId colorize_regex_factory(HighlighterParameters params)
{
    if (params.size() < 2)
        throw runtime_error("wrong parameter count");

    try
    {
        static Regex color_spec_ex(R"((\d+):(\w+(,\w+)?))");
        ColorSpec colors;
        for (auto it = params.begin() + 1;  it != params.end(); ++it)
        {
            boost::smatch res;
            if (not boost::regex_match(it->begin(), it->end(), res, color_spec_ex))
                throw runtime_error("wrong colorspec: '" + *it +
                                     "' expected <capture>:<fgcolor>[,<bgcolor>]");

            int capture = str_to_int(res[1].str());
            const ColorPair*& color = colors[capture];
            color = &get_color(res[2].str());
        }

        String id = "colre'" + params[0] + "'";

        Regex ex{params[0].begin(), params[0].end(), boost::regex::optimize};

        return HighlighterAndId(id, RegexColorizer(std::move(ex),
                                                   std::move(colors)));
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error(String("regex error: ") + err.what());
    }
}

template<typename RegexGetter>
class DynamicRegexHighlighter
{
public:
    DynamicRegexHighlighter(const ColorSpec& colors, RegexGetter getter)
        : m_regex_getter(getter), m_colors(colors), m_colorizer(Regex(), m_colors) {}

    void operator()(const Context& context, DisplayBuffer& display_buffer)
    {
        Regex regex = m_regex_getter(context);
        if (regex != m_last_regex)
        {
            m_last_regex = regex;
            if (not m_last_regex.empty())
                m_colorizer = RegexColorizer{m_last_regex, m_colors};
        }
        if (not m_last_regex.empty())
            m_colorizer(context, display_buffer);
    }

private:
    Regex          m_last_regex;
    ColorSpec      m_colors;
    RegexColorizer m_colorizer;
    RegexGetter    m_regex_getter;
};

HighlighterAndId highlight_search_factory(HighlighterParameters params)
{
    if (params.size() != 1)
        throw runtime_error("wrong parameter count");
    try
    {
        ColorSpec colors { { 0, &get_color(params[0]) } };
        auto get_regex = [](const Context&){
            auto s = RegisterManager::instance()['/'].values(Context{});
            return s.empty() ? Regex{} : Regex{s[0].begin(), s[0].end()};
        };
        return {"hlsearch", DynamicRegexHighlighter<decltype(get_regex)>{colors, get_regex}};
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error(String("regex error: ") + err.what());
    }
}

HighlighterAndId highlight_regex_option_factory(HighlighterParameters params)
{
    if (params.size() != 2)
        throw runtime_error("wrong parameter count");

    ColorSpec colors { { 0, &get_color(params[1]) } };
    String option_name = params[0];
    // verify option type now
    GlobalOptions::instance()[option_name].get<Regex>();

    auto get_regex = [option_name](const Context& context){ return context.options()[option_name].get<Regex>(); };
    return {"hloption_" + option_name, DynamicRegexHighlighter<decltype(get_regex)>{colors, get_regex}};
}

void expand_tabulations(const Context& context, DisplayBuffer& display_buffer)
{
    const int tabstop = context.options()["tabstop"].get<int>();
    auto& buffer = context.buffer();
    for (auto& line : display_buffer.lines())
    {
        for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
        {
            if (atom_it->type() != DisplayAtom::BufferRange)
                continue;

            auto begin = buffer.iterator_at(atom_it->begin());
            auto end = buffer.iterator_at(atom_it->end());
            for (BufferIterator it = begin; it != end; ++it)
            {
                if (*it == '\t')
                {
                    if (it != begin)
                        atom_it = ++line.split(atom_it, it.coord());
                    if (it+1 != end)
                        atom_it = line.split(atom_it, (it+1).coord());

                    int column = 0;
                    for (auto line_it = buffer.iterator_at(it.coord().line);
                         line_it != it; ++line_it)
                    {
                        kak_assert(*line_it != '\n');
                        if (*line_it == '\t')
                            column += tabstop - (column % tabstop);
                        else
                           ++column;
                    }

                    int count = tabstop - (column % tabstop);
                    String padding;
                    for (int i = 0; i < count; ++i)
                        padding += ' ';
                    atom_it->replace(padding);
                    break;
                }
            }
        }
    }
}

void show_line_numbers(const Context& context, DisplayBuffer& display_buffer)
{
    LineCount last_line = context.buffer().line_count();
    int digit_count = 0;
    for (LineCount c = last_line; c > 0; c /= 10)
        ++digit_count;

    char format[] = "%?d│";
    format[1] = '0' + digit_count;
    auto& colors = get_color("LineNumbers");
    for (auto& line : display_buffer.lines())
    {
        char buffer[10];
        snprintf(buffer, 10, format, (int)line.range().first.line + 1);
        DisplayAtom atom{buffer};
        atom.colors = colors;
        line.insert(line.begin(), std::move(atom));
    }
}

void highlight_selections(const Context& context, DisplayBuffer& display_buffer)
{
    const auto& buffer = context.buffer();
    for (size_t i = 0; i < context.selections().size(); ++i)
    {
        auto& sel = context.selections()[i];
        const bool forward = sel.first() <= sel.last();
        BufferCoord begin = forward ? sel.first() : buffer.char_next(sel.last());
        BufferCoord end   = forward ? sel.last() : buffer.char_next(sel.first());

        const bool primary = (i == context.selections().main_index());
        ColorPair sel_colors = get_color(primary ? "PrimarySelection" : "SecondarySelection");
        highlight_range(display_buffer, begin, end, false,
                        [&](DisplayAtom& atom) { atom.colors = sel_colors; });
        ColorPair cur_colors = get_color(primary ? "PrimaryCursor" : "SecondaryCursor");
        highlight_range(display_buffer, sel.last(), buffer.char_next(sel.last()), false,
                        [&](DisplayAtom& atom) { atom.colors = cur_colors; });
    }
}

void expand_unprintable(const Context& context, DisplayBuffer& display_buffer)
{
    auto& buffer = context.buffer();
    for (auto& line : display_buffer.lines())
    {
        for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
        {
            if (atom_it->type() == DisplayAtom::BufferRange)
            {
                using Utf8It = utf8::utf8_iterator<BufferIterator, utf8::InvalidBytePolicy::Pass>;
                for (Utf8It it  = buffer.iterator_at(atom_it->begin()),
                            end = buffer.iterator_at(atom_it->end()); it != end; ++it)
                {
                    Codepoint cp = *it;
                    if (cp != '\n' and not iswprint(cp))
                    {
                        std::ostringstream oss;
                        oss << "U+" << std::hex << cp;
                        String str = oss.str();
                        if (it.base().coord() != atom_it->begin())
                            atom_it = ++line.split(atom_it, it.base().coord());
                        if ((it+1).base().coord() != atom_it->end())
                            atom_it = line.split(atom_it, (it+1).base().coord());
                        atom_it->replace(str);
                        atom_it->colors = { Colors::Red, Colors::Black };
                        break;
                    }
                }
            }
        }
    }
}

HighlighterAndId flag_lines_factory(HighlighterParameters params)
{
    if (params.size() != 2)
        throw runtime_error("wrong parameter count");

    const String& option_name = params[1];
    Color bg = str_to_color(params[0]);

    // throw if wrong option type
    GlobalOptions::instance()[option_name].get<std::vector<LineAndFlag>>();

    return {"hlflags_" + params[1],
            [=](const Context& context, DisplayBuffer& display_buffer)
            {
                auto& lines_opt = context.options()[option_name];
                auto& lines = lines_opt.get<std::vector<LineAndFlag>>();

                CharCount width = 0;
                for (auto& l : lines)
                     width = std::max(width, std::get<2>(l).char_length());
                const String empty{' ', width};
                for (auto& line : display_buffer.lines())
                {
                    int line_num = (int)line.range().first.line + 1;
                    auto it = find_if(lines,
                                      [&](const LineAndFlag& l)
                                      { return std::get<0>(l) == line_num; });
                    String content = it != lines.end() ? std::get<2>(*it) : empty;
                    content += String(' ', width - content.char_length());
                    DisplayAtom atom{std::move(content)};
                    atom.colors = { it != lines.end() ? std::get<1>(*it) : Colors::Default , bg };
                    line.insert(line.begin(), std::move(atom));
                }
            }};
}

template<void (*highlighter_func)(const Context&, DisplayBuffer&)>
class SimpleHighlighterFactory
{
public:
    SimpleHighlighterFactory(const String& id) : m_id(id) {}

    HighlighterAndId operator()(HighlighterParameters params) const
    {
        return HighlighterAndId(m_id, HighlighterFunc(highlighter_func));
    }
private:
    String m_id;
};

HighlighterAndId highlighter_group_factory(HighlighterParameters params)
{
    if (params.size() != 1)
        throw runtime_error("wrong parameter count");

    return HighlighterAndId(params[0], HighlighterGroup());
}

HighlighterAndId reference_factory(HighlighterParameters params)
{
    if (params.size() != 1)
        throw runtime_error("wrong parameter count");

    const String& name = params[0];

    // throw if not found
    DefinedHighlighters::instance().get_group(name, '/');

    return HighlighterAndId(name,
                            [name](const Context& context, DisplayBuffer& display_buffer)
                            { DefinedHighlighters::instance().get_group(name, '/')(context, display_buffer); });
}

template<typename HighlightFunc>
struct RegionHighlighter
{
public:
    RegionHighlighter(Regex begin, Regex end, HighlightFunc func)
        : m_begin(std::move(begin)),
          m_end(std::move(end)),
          m_func(std::move(func)),
          m_cache_id{ValueId::get_free_id()}
    {}

    void operator()(const Context& context, DisplayBuffer& display_buffer)
    {
        auto& cache = update_cache_ifn(context.buffer());
        for (auto& pair : cache.regions)
            m_func(context, display_buffer, pair.first, pair.second);
    }
private:
    Regex m_begin;
    Regex m_end;
    HighlightFunc m_func;
    ValueId m_cache_id;

    struct RegionCache
    {
        size_t timestamp = -1;
        std::vector<std::pair<BufferCoord, BufferCoord>> regions;
    };

    RegionCache& update_cache_ifn(const Buffer& buffer)
    {
        Value& cache_val = buffer.values()[m_cache_id];
        if (not cache_val)
            cache_val = Value(RegionCache{});
        RegionCache& cache = cache_val.as<RegionCache>();

        if (cache.timestamp == buffer.timestamp())
            return cache;

        cache.regions.clear();

        boost::match_results<BufferIterator> results;
        auto pos = buffer.begin();
        auto end = buffer.end();
        while (boost::regex_search(pos, end, results, m_begin))
        {
            pos = results[0].first;
            if (boost::regex_search(results[0].second, end, results, m_end))
            {
                cache.regions.emplace_back(pos.coord(), results[0].second.coord());
                pos = results[0].second;
            }
            else
                break;
        }
        cache.timestamp = buffer.timestamp();
        return cache;
    }
};

template<typename HighlightFunc>
RegionHighlighter<HighlightFunc>
make_region_highlighter(Regex begin, Regex end, HighlightFunc func)
{
    return RegionHighlighter<HighlightFunc>(std::move(begin), std::move(end), std::move(func));
}

HighlighterAndId region_factory(HighlighterParameters params)
{
    try
    {
        if (params.size() != 3)
            throw runtime_error("wrong parameter count");

        Regex begin{params[0], boost::regex::nosubs | boost::regex::optimize };
        Regex end{params[1], boost::regex::nosubs | boost::regex::optimize };
        const ColorPair colors = get_color(params[2]);

        auto func = [colors](const Context&, DisplayBuffer& display_buffer,
                             BufferCoord begin, BufferCoord end)
        {
            highlight_range(display_buffer, begin, end, true,
                            [&colors](DisplayAtom& atom) { atom.colors = colors; });
        };

        return HighlighterAndId("region(" + params[0] + "," + params[1] + ")",
                                make_region_highlighter(std::move(begin), std::move(end), func));
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error(String("regex error: ") + err.what());
    }
}

HighlighterAndId region_ref_factory(HighlighterParameters params)
{
    try
    {
        if (params.size() != 3 and params.size() != 4)
            throw runtime_error("wrong parameter count");

        Regex begin{params[0], boost::regex::nosubs | boost::regex::optimize };
        Regex end{params[1], boost::regex::nosubs | boost::regex::optimize };
        const String& name = params[2];

        auto func = [name](const Context& context, DisplayBuffer& display_buffer,
                          BufferCoord begin, BufferCoord end)
        {
            HighlighterGroup& ref = DefinedHighlighters::instance().get_group(name, '/');
            apply_highlighter(context, display_buffer, begin, end, ref);
        };

        return HighlighterAndId("regionref(" + params[0] + "," + params[1] + "," + name + ")",
                                make_region_highlighter(std::move(begin), std::move(end), func));
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error(String("regex error: ") + err.what());
    }
}

void register_highlighters()
{
    HighlighterRegistry& registry = HighlighterRegistry::instance();

    registry.register_func("number_lines", SimpleHighlighterFactory<show_line_numbers>("number_lines"));
    registry.register_func("regex", colorize_regex_factory);
    registry.register_func("regex_option", highlight_regex_option_factory);
    registry.register_func("search", highlight_search_factory);
    registry.register_func("group", highlighter_group_factory);
    registry.register_func("flag_lines", flag_lines_factory);
    registry.register_func("ref", reference_factory);
    registry.register_func("region", region_factory);
    registry.register_func("region_ref", region_ref_factory);
}

}
