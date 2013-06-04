#include "highlighters.hh"

#include "assert.hh"
#include "color_registry.hh"
#include "context.hh"
#include "option_types.hh"
#include "register_manager.hh"
#include "string.hh"
#include "utf8.hh"
#include "utf8_iterator.hh"
#include "window.hh"

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
        if (line.buffer_line() < begin.line or  end.line < line.buffer_line())
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

    void operator()(const Window& window, DisplayBuffer& display_buffer)
    {
        update_cache_ifn(window.buffer(), display_buffer.range());
        for (auto& match : m_cache_matches)
        {
            for (size_t n = 0; n < match.size(); ++n)
            {
                auto col_it = m_colors.find(n);
                if (col_it == m_colors.end())
                    continue;

                highlight_range(display_buffer, match[n].first.coord(), match[n].second.coord(), true,
                                [&](DisplayAtom& atom) { atom.colors = *col_it->second; });
            }
        }
    }

private:
    BufferRange m_cache_range;
    size_t      m_cache_timestamp;
    std::vector<boost::match_results<BufferIterator>> m_cache_matches;

    Regex     m_regex;
    ColorSpec m_colors;

    void update_cache_ifn(const Buffer& buffer, const BufferRange& range)
    {
        if (buffer.timestamp() == m_cache_timestamp and
            range.first >= m_cache_range.first and
            range.second <= m_cache_range.second)
           return;

        m_cache_matches.clear();
        m_cache_range.first  = buffer.clamp({range.first.line - 10, 0});
        m_cache_range.second = buffer.clamp({range.second.line + 10, INT_MAX});
        m_cache_timestamp = buffer.timestamp();

        RegexIterator re_it{buffer.iterator_at(m_cache_range.first),
                            buffer.iterator_at(m_cache_range.second), m_regex};
        RegexIterator re_end;
        for (; re_it != re_end; ++re_it)
            m_cache_matches.push_back(*re_it);
    }
};

HighlighterAndId colorize_regex_factory(const HighlighterParameters params, const Window&)
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

template<typename RegexGetter>
class DynamicRegexHighlighter
{
public:
    DynamicRegexHighlighter(const ColorSpec& colors, RegexGetter getter)
        : m_regex_getter(getter), m_colors(colors), m_colorizer(Regex(), m_colors) {}

    void operator()(const Window& window, DisplayBuffer& display_buffer)
    {
        Regex regex = m_regex_getter();
        if (regex != m_last_regex)
        {
            m_last_regex = regex;
            if (not m_last_regex.empty())
                m_colorizer = RegexColorizer{m_last_regex, m_colors};
        }
        if (not m_last_regex.empty())
            m_colorizer(window, display_buffer);
    }

private:
    Regex          m_last_regex;
    ColorSpec      m_colors;
    RegexColorizer m_colorizer;
    RegexGetter    m_regex_getter;
};

HighlighterAndId highlight_search_factory(const HighlighterParameters params, const Window&)
{
    if (params.size() != 1)
        throw runtime_error("wrong parameter count");
    try
    {
        ColorSpec colors { { 0, &get_color(params[0]) } };
        auto get_regex = []{
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

HighlighterAndId highlight_regex_option_factory(const HighlighterParameters params, const Window& window)
{
    if (params.size() != 2)
        throw runtime_error("wrong parameter count");

    ColorSpec colors { { 0, &get_color(params[1]) } };
    String option_name = params[0];
    const OptionManager& options = window.options();
    // verify option type now
    options[option_name].get<Regex>();

    auto get_regex = [option_name, &options]{ return options[option_name].get<Regex>(); };
    return {"hloption_" + option_name, DynamicRegexHighlighter<decltype(get_regex)>{colors, get_regex}};
}

void expand_tabulations(const Window& window, DisplayBuffer& display_buffer)
{
    const int tabstop = window.options()["tabstop"].get<int>();
    auto& buffer = window.buffer();
    for (auto& line : display_buffer.lines())
    {
        for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
        {
            if (atom_it->content.type() != AtomContent::BufferRange)
                continue;

            auto begin = buffer.iterator_at(atom_it->content.begin());
            auto end = buffer.iterator_at(atom_it->content.end());
            for (BufferIterator it = begin; it != end; ++it)
            {
                if (*it == '\t')
                {
                    if (it != begin)
                        atom_it = ++line.split(atom_it, it.coord());
                    if (it+1 != end)
                        atom_it = line.split(atom_it, (it+1).coord());

                    int column = 0;
                    for (auto line_it = buffer.iterator_at(line.buffer_line());
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
                    atom_it->content.replace(padding);
                    break;
                }
            }
        }
    }
}

void show_line_numbers(const Window& window, DisplayBuffer& display_buffer)
{
    LineCount last_line = window.buffer().line_count();
    int digit_count = 0;
    for (LineCount c = last_line; c > 0; c /= 10)
        ++digit_count;

    char format[] = "%?d│";
    format[1] = '0' + digit_count;
    auto& colors = get_color("LineNumbers");
    for (auto& line : display_buffer.lines())
    {
        char buffer[10];
        snprintf(buffer, 10, format, (int)line.buffer_line() + 1);
        DisplayAtom atom = DisplayAtom(AtomContent(buffer));
        atom.colors = colors;
        line.insert(line.begin(), std::move(atom));
    }
}

void highlight_selections(const Window& window, DisplayBuffer& display_buffer)
{
    const bool only_cursor = window.is_editing() and window.options()["insert_hide_sel"].get<bool>();
    const auto& buffer = window.buffer();
    for (size_t i = 0; i < window.selections().size(); ++i)
    {
        auto& sel = window.selections()[i];
        const bool forward = sel.first() <= sel.last();
        BufferCoord begin = forward ? sel.first() : buffer.char_next(sel.last());
        BufferCoord end   = forward ? sel.last() : buffer.char_next(sel.first());

        const bool primary = (i == window.main_selection_index());
        if (not only_cursor)
        {
            ColorPair sel_colors = get_color(primary ? "PrimarySelection" : "SecondarySelection");
            highlight_range(display_buffer, begin, end, false,
                            [&](DisplayAtom& atom) { atom.colors = sel_colors; });
        }
        ColorPair cur_colors = get_color(primary ? "PrimaryCursor" : "SecondaryCursor");
        highlight_range(display_buffer, sel.last(), buffer.char_next(sel.last()), false,
                        [&](DisplayAtom& atom) { atom.colors = cur_colors; });
    }
}

void expand_unprintable(const Window& window, DisplayBuffer& display_buffer)
{
    auto& buffer = window.buffer();
    for (auto& line : display_buffer.lines())
    {
        for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
        {
            if (atom_it->content.type() == AtomContent::BufferRange)
            {
                using Utf8It = utf8::utf8_iterator<BufferIterator, utf8::InvalidBytePolicy::Pass>;
                for (Utf8It it  = buffer.iterator_at(atom_it->content.begin()),
                            end = buffer.iterator_at(atom_it->content.end()); it != end; ++it)
                {
                    Codepoint cp = *it;
                    if (cp != '\n' and not std::isprint((wchar_t)cp, std::locale()))
                    {
                        std::ostringstream oss;
                        oss << "U+" << std::hex << cp;
                        String str = oss.str();
                        if (it.underlying_iterator().coord() != atom_it->content.begin())
                            atom_it = ++line.split(atom_it, it.underlying_iterator().coord());
                        if ((it+1).underlying_iterator().coord() != atom_it->content.end())
                            atom_it = line.split(atom_it, (it+1).underlying_iterator().coord());
                        atom_it->content.replace(str);
                        atom_it->colors = { Colors::Red, Colors::Black };
                        break;
                    }
                }
            }
        }
    }
}

class FlagLines : public OptionManagerWatcher_AutoRegister
{
public:
    FlagLines(Color bg, String option_name, Window& window)
        : OptionManagerWatcher_AutoRegister(window.options()),
          m_bg(bg), m_option_name(std::move(option_name)),
          m_window(window)
    {
        // trigger an exception if option is not of right type.
        m_window.options()[m_option_name].get<std::vector<LineAndFlag>>();
        update_shared_option_updater();
    }

    void operator()(const Window&, DisplayBuffer& display_buffer)
    {
        update_shared_option_updater();
        auto& lines = m_window.options()[m_option_name].get<std::vector<LineAndFlag>>();

        CharCount width = 0;
        for (auto& l : lines)
             width = std::max(width, std::get<2>(l).char_length());
        const String empty{' ', width};
        for (auto& line : display_buffer.lines())
        {
            int line_num = (int)line.buffer_line() + 1;
            auto it = find_if(lines, [&](const LineAndFlag& l) { return std::get<0>(l) == line_num; });
            String content = it != lines.end() ? std::get<2>(*it) : empty;
            content += String(' ', width - content.char_length());
            DisplayAtom atom{AtomContent(std::move(content))};
            atom.colors = { it != lines.end() ? std::get<1>(*it) : Colors::Default , m_bg };
            line.insert(line.begin(), std::move(atom));
        }
    }

private:
    void on_option_changed(const Option& option) override
    {
        if (option.name() == m_option_name)
            m_window.forget_timestamp();
    }

    struct OptionUpdater : BufferChangeListener_AutoRegister
    {
        OptionUpdater(Buffer& buffer, Option& option)
            : BufferChangeListener_AutoRegister(buffer), m_option(&option)
        {
            // sanity checks
            kak_assert(&m_option->manager() != &GlobalOptions::instance());
            m_option->get<std::vector<LineAndFlag>>();
        }

        void on_insert(const Buffer&, const BufferCoord& begin, const BufferCoord& end) override
        {
            LineCount new_lines = end.line - begin.line;
            if (new_lines == 0)
                return;

            auto lines = m_option->get<std::vector<LineAndFlag>>();
            for (auto& line : lines)
            {
                if (std::get<0>(line) > begin.line)
                    std::get<0>(line) += new_lines;
            }
            m_option->set(lines);
        }

        void on_erase(const Buffer&, const BufferCoord& begin, const BufferCoord& end)  override
        {
            LineCount removed_lines = end.line - begin.line;
            if (removed_lines == 0)
                return;

            auto lines = m_option->get<std::vector<LineAndFlag>>();
            for (auto& line : lines)
            {
                if (std::get<0>(line) > begin.line)
                    std::get<0>(line) -= removed_lines;
            }
            m_option->set(lines);
        }

        const Option& option() const { return *m_option; }
    private:
        safe_ptr<Option> m_option;
    };
    static std::unordered_map<const Option*, std::weak_ptr<OptionUpdater>> ms_updaters;

    void update_shared_option_updater()
    {
        const Option& option = m_window.options()[m_option_name];
        if (&option.manager() == &GlobalOptions::instance() or
            (m_updater and &option == &m_updater->option()))
            return;
        auto& shared_updater = ms_updaters[&option];
        if (shared_updater.expired())
        {
            m_updater = std::make_shared<OptionUpdater>(
                m_window.buffer(), option.manager().get_local_option(option.name()));
            shared_updater = m_updater;
        }
        else
           m_updater = shared_updater.lock();
    }

    Color m_bg;
    String m_option_name;
    Window& m_window;
    std::shared_ptr<OptionUpdater> m_updater;
};
std::unordered_map<const Option*, std::weak_ptr<FlagLines::OptionUpdater>> FlagLines::ms_updaters;

HighlighterAndId flag_lines_factory(const HighlighterParameters& params, Window& window)
{
    if (params.size() != 2)
        throw runtime_error("wrong parameter count");

    return {"hlflags_" + params[1], FlagLines{str_to_color(params[0]), params[1], window}};
}

template<void (*highlighter_func)(const Window&, DisplayBuffer&)>
class SimpleHighlighterFactory
{
public:
    SimpleHighlighterFactory(const String& id) : m_id(id) {}

    HighlighterAndId operator()(const HighlighterParameters& params, const Window&) const
    {
        return HighlighterAndId(m_id, HighlighterFunc(highlighter_func));
    }
private:
    String m_id;
};

HighlighterAndId highlighter_group_factory(const HighlighterParameters& params, const Window&)
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
    registry.register_func("regex_option", highlight_regex_option_factory);
    registry.register_func("search", highlight_search_factory);
    registry.register_func("group", highlighter_group_factory);
    registry.register_func("flag_lines", flag_lines_factory);
}

}
