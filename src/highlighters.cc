#include "highlighters.hh"

#include "assert.hh"
#include "buffer_utils.hh"
#include "changes.hh"
#include "command_manager.hh"
#include "context.hh"
#include "clock.hh"
#include "display_buffer.hh"
#include "face_registry.hh"
#include "highlighter_group.hh"
#include "line_modification.hh"
#include "option_types.hh"
#include "parameters_parser.hh"
#include "ranges.hh"
#include "regex.hh"
#include "register_manager.hh"
#include "string.hh"
#include "utf8.hh"
#include "utf8_iterator.hh"
#include "window.hh"

#include <cstdio>
#include <limits>

namespace Kakoune
{

using Utf8Iterator = utf8::iterator<BufferIterator>;

template<typename Func>
std::unique_ptr<Highlighter> make_highlighter(Func func, HighlightPass pass = HighlightPass::Colorize)
{
    struct SimpleHighlighter : public Highlighter
    {
        SimpleHighlighter(Func func, HighlightPass pass)
          : Highlighter{pass}, m_func{std::move(func)} {}

    private:
        void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange range) override
        {
            m_func(context, display_buffer, range);
        }
        Func m_func;
    };
    return std::make_unique<SimpleHighlighter>(std::move(func), pass);
}

template<typename T>
void highlight_range(DisplayBuffer& display_buffer,
                     BufferCoord begin, BufferCoord end,
                     bool skip_replaced, T func)
{
    // tolerate begin > end as that can be triggered by wrong encodings
    if (begin >= end or end <= display_buffer.range().begin
                     or begin >= display_buffer.range().end)
        return;

    for (auto& line : display_buffer.lines())
    {
        auto& range = line.range();
        if (range.end <= begin or end < range.begin)
            continue;

        for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
        {
            bool is_replaced = atom_it->type() == DisplayAtom::ReplacedRange;

            if (not atom_it->has_buffer_range() or
                (skip_replaced and is_replaced) or
                end <= atom_it->begin() or begin >= atom_it->end())
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
void replace_range(DisplayBuffer& display_buffer,
                   BufferCoord begin, BufferCoord end, T func)
{
    // tolerate begin > end as that can be triggered by wrong encodings
    if (begin > end or end < display_buffer.range().begin or begin > display_buffer.range().end)
        return;

    auto& lines = display_buffer.lines();
    auto first_it = std::lower_bound(lines.begin(), lines.end(), begin, [](const DisplayLine& l, const BufferCoord& c) { return l.range().end < c; });
    if (first_it == lines.end())
        return;

    auto first_atom_it = std::find_if(first_it->begin(), first_it->end(), [&begin](const DisplayAtom& a) { return a.has_buffer_range() and a.end() > begin; });
    first_atom_it = first_it->split(begin);

    auto last_it = std::lower_bound(first_it, lines.end(), end, [](const DisplayLine& l, const BufferCoord& c) { return l.range().end < c; });

    if (first_it == last_it)
    {
        auto first_atom_idx = first_atom_it - first_it->begin();
        auto end_atom_it = first_it->split(end);
        first_atom_it = first_it->erase(first_it->begin() + first_atom_idx, end_atom_it);
    }
    else
    {
        first_atom_it = first_it->erase(first_atom_it, first_it->end());
        if (last_it != lines.end())
        {
            auto end_atom_it = last_it->split(end);
            end_atom_it = last_it->erase(last_it->begin(), end_atom_it);

            first_atom_it = first_it->insert(first_atom_it, end_atom_it, last_it->end());
            ++last_it;
        }
        first_it = --lines.erase(first_it+1, last_it);
    }

    func(*first_it, first_atom_it);
}

void apply_highlighter(HighlightContext context,
                       DisplayBuffer& display_buffer,
                       BufferCoord begin, BufferCoord end,
                       Highlighter& highlighter)
{
    if (begin == end)
        return;

    using LineIterator = DisplayLineList::iterator;
    LineIterator first_line;
    Vector<size_t> insert_idx;
    auto line_end = display_buffer.lines().end();

    DisplayBuffer region_display;
    auto& region_lines = region_display.lines();
    for (auto line_it = display_buffer.lines().begin(); line_it != line_end; ++line_it)
    {
        auto& line = *line_it;
        auto& range = line.range();
        if (range.end <= begin or end <= range.begin)
            continue;

        if (region_lines.empty())
            first_line = line_it;

        if (range.begin < begin or range.end > end)
        {
            size_t beg_idx = 0;
            size_t end_idx = line.atoms().size();

            for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
            {
                if (not atom_it->has_buffer_range() or end <= atom_it->begin() or begin >= atom_it->end())
                    continue;

                bool is_replaced = atom_it->type() == DisplayAtom::ReplacedRange;
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
                        end_idx = atom_it - line.begin() + 1;
                    else
                    {
                        atom_it = ++line.split(atom_it, end);
                        end_idx = atom_it - line.begin();
                    }
                }
            }
            region_lines.emplace_back();
            std::move(line.begin() + beg_idx, line.begin() + end_idx,
                      std::back_inserter(region_lines.back()));
            auto it = line.erase(line.begin() + beg_idx, line.begin() + end_idx);
            insert_idx.push_back(it - line.begin());
        }
        else
        {
            insert_idx.push_back(0);
            region_lines.push_back(std::move(line));
        }
    }

    if (region_display.lines().empty())
        return;

    region_display.compute_range();
    highlighter.highlight(context, region_display, {begin, end});

    for (size_t i = 0; i < region_lines.size(); ++i)
    {
        auto& line = *(first_line + i);
        auto pos = line.begin() + insert_idx[i];
        for (auto& atom : region_lines[i])
            pos = ++line.insert(pos, std::move(atom));
    }
    display_buffer.compute_range();
}

auto apply_face = [](const Face& face)
{
    return [&face](DisplayAtom& atom) {
        atom.face = merge_faces(atom.face, face);
    };
};

const HighlighterDesc fill_desc = {
    "Fill the whole highlighted range with the given face",
    {}
};
static std::unique_ptr<Highlighter> create_fill_highlighter(HighlighterParameters params, Highlighter*)
{
    if (params.size() != 1)
        throw runtime_error("wrong parameter count");

    const String& facespec = params[0];
    auto func = [facespec](HighlightContext context, DisplayBuffer& display_buffer, BufferRange range)
    {
        highlight_range(display_buffer, range.begin, range.end, false,
                        apply_face(context.context.faces()[facespec]));
    };
    return make_highlighter(std::move(func));
}

template<typename T>
struct BufferSideCache
{
    BufferSideCache() : m_id{get_free_value_id()} {}

    T& get(const Buffer& buffer)
    {
        Value& cache_val = buffer.values()[m_id];
        if (not cache_val)
            cache_val = Value(T{});
        return cache_val.as<T>();
    }
private:
    ValueId m_id;
};

using FacesSpec = Vector<std::pair<size_t, String>, MemoryDomain::Highlight>;

const HighlighterDesc regex_desc = {
    "Parameters: <regex> <capture num>:<face> <capture num>:<face>...\n"
    "Highlights the matches for captures from the regex with the given faces",
    {}
};
class RegexHighlighter : public Highlighter
{
public:
    RegexHighlighter(Regex regex, FacesSpec faces)
        : Highlighter{HighlightPass::Colorize},
          m_regex{std::move(regex)},
          m_faces{std::move(faces)}
    {
        ensure_first_face_is_capture_0();
    }

    void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange range) override
    {
        auto overlaps = [](const BufferRange& lhs, const BufferRange& rhs) {
            return lhs.begin < rhs.begin ? lhs.end > rhs.begin
                                         : rhs.end > lhs.begin;
        };

        if (not overlaps(display_buffer.range(), range))
            return;

        const auto faces = m_faces | transform([&faces = context.context.faces()](auto&& spec) {
                return spec.second.empty() ? Face{} : faces[spec.second];
            }) | gather<Vector<Face>>();

        const auto& matches = get_matches(context.context.buffer(), display_buffer.range(), range);
        kak_assert(matches.size() % m_faces.size() == 0);
        for (size_t m = 0; m < matches.size(); ++m)
        {
            auto& face = faces[m % faces.size()];
            if (face == Face{})
                continue;

            highlight_range(display_buffer,
                            matches[m].begin, matches[m].end,
                            false, apply_face(face));
        }
    }

    void reset(Regex regex, FacesSpec faces)
    {
        m_regex = std::move(regex);
        m_faces = std::move(faces);
        ensure_first_face_is_capture_0();
        ++m_regex_version;
    }

    static std::unique_ptr<Highlighter> create(HighlighterParameters params, Highlighter*)
    {
        if (params.size() < 2)
            throw runtime_error("wrong parameter count");

        Regex re{params[0], RegexCompileFlags::Optimize};

        FacesSpec faces;
        for (auto& spec : params.subrange(1))
        {
            auto colon = find(spec, ':');
            if (colon == spec.end())
                throw runtime_error(format("wrong face spec: '{}' expected <capture>:<facespec>", spec));
            const StringView capture_name{spec.begin(), colon};
            const int capture = str_to_int_ifp(capture_name).value_or_compute([&] {
                return re.named_capture_index(capture_name);
            });
            if (capture < 0)
                throw runtime_error(format("capture name {} is neither a capture index, nor an existing capture name",
                                           capture_name));
            faces.emplace_back(capture, String{colon+1, spec.end()});
        }

        return std::make_unique<RegexHighlighter>(std::move(re), std::move(faces));
    }

private:
    // stores the range for each highlighted capture of each match
    using MatchList = Vector<BufferRange, MemoryDomain::Highlight>;
    struct Cache
    {
        size_t m_timestamp = -1;
        size_t m_regex_version = -1;
        struct RangeAndMatches { BufferRange range; MatchList matches; };
        using RangeAndMatchesList = Vector<RangeAndMatches, MemoryDomain::Highlight>;
        HashMap<BufferRange, RangeAndMatchesList, MemoryDomain::Highlight> m_matches;
    };
    BufferSideCache<Cache> m_cache;

    Regex     m_regex;
    FacesSpec m_faces;

    size_t m_regex_version = 0;

    void ensure_first_face_is_capture_0()
    {
        if (m_faces.empty())
            return;

        std::sort(m_faces.begin(), m_faces.end(),
                  [](const std::pair<size_t, String>& lhs,
                     const std::pair<size_t, String>& rhs)
                  { return lhs.first < rhs.first; });
        if (m_faces[0].first != 0)
            m_faces.emplace(m_faces.begin(), 0, String{});
    }

    void add_matches(const Buffer& buffer, MatchList& matches, BufferRange range)
    {
        kak_assert(matches.size() % m_faces.size() == 0);
        for (auto&& match : RegexIterator{get_iterator(buffer, range.begin),
                                          get_iterator(buffer, range.end),
                                          buffer.begin(), buffer.end(), m_regex,
                                          match_flags(is_bol(range.begin),
                                                      is_eol(buffer, range.end),
                                                      is_bow(buffer, range.begin),
                                                      is_eow(buffer, range.end))})
        {
            for (auto& face : m_faces)
            {
                const auto& sub = match[face.first];
                matches.push_back({sub.first.coord(), sub.second.coord()});
            }
        }
    }

    const MatchList& get_matches(const Buffer& buffer, BufferRange display_range, BufferRange buffer_range)
    {
        Cache& cache = m_cache.get(buffer);

        if (cache.m_regex_version != m_regex_version or
            cache.m_timestamp != buffer.timestamp() or
            accumulate(cache.m_matches, (size_t)0, [](size_t c, auto&& m) { return c + m.value.size(); }) > 1000)
        {
            cache.m_matches.clear();
            cache.m_timestamp = buffer.timestamp();
            cache.m_regex_version = m_regex_version;
        }

        auto& matches = cache.m_matches[buffer_range];

        const LineCount line_offset = 3;
        BufferRange range{std::max<BufferCoord>(buffer_range.begin, display_range.begin.line - line_offset),
                          std::min<BufferCoord>(buffer_range.end, display_range.end.line + line_offset)};

        auto it = std::upper_bound(matches.begin(), matches.end(), range.begin,
                                   [](const BufferCoord& lhs, const Cache::RangeAndMatches& rhs)
                                   { return lhs < rhs.range.end; });

        if (it == matches.end() or it->range.begin > range.end)
        {
            it = matches.insert(it, Cache::RangeAndMatches{range, {}});
            add_matches(buffer, it->matches, range);
        }
        else if (it->matches.empty())
        {
            it->range = range;
            add_matches(buffer, it->matches, range);
        }
        else
        {
            // Here we extend the matches, that is not strictly valid,
            // but may work nicely with every reasonable regex, and
            // greatly reduces regex parsing. To change if we encounter
            // regex that do not work great with that.
            BufferRange& old_range = it->range;
            MatchList& matches = it->matches;

            // Thanks to the ensure_first_face_is_capture_0 method, we know
            // these point to the first/last matches capture 0.
            auto first_end = matches.begin()->end;
            auto last_end = (matches.end() - m_faces.size())->end;

            // add regex matches from new begin to old first match end
            if (range.begin < old_range.begin)
            {
                matches.erase(matches.begin(), matches.begin() + m_faces.size());
                size_t pivot = matches.size();
                old_range.begin = range.begin;
                add_matches(buffer, matches, {range.begin, first_end});

                std::rotate(matches.begin(), matches.begin() + pivot, matches.end());
            }
            // add regex matches from old last match begin to new end
            if (old_range.end < range.end)
            {
                old_range.end = range.end;
                add_matches(buffer, matches, {last_end, range.end});
            }
        }
        return it->matches;
    }
};

template<typename RegexGetter, typename FaceGetter>
class DynamicRegexHighlighter : public Highlighter
{
public:
    DynamicRegexHighlighter(RegexGetter regex_getter, FaceGetter face_getter)
      : Highlighter{HighlightPass::Colorize},
        m_regex_getter(std::move(regex_getter)),
        m_face_getter(std::move(face_getter)),
        m_highlighter(Regex{}, FacesSpec{}) {}

    void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange range) override
    {
        Regex regex = m_regex_getter(context.context);
        FacesSpec face = regex.empty() ? FacesSpec{} : m_face_getter(context.context, regex);
        if (regex != m_last_regex or face != m_last_face)
        {
            m_last_regex = std::move(regex);
            m_last_face = face;
            if (not m_last_regex.empty())
                m_highlighter.reset(m_last_regex, m_last_face);
        }
        if (not m_last_regex.empty() and not m_last_face.empty())
            m_highlighter.highlight(context, display_buffer, range);
    }

private:
    Regex       m_last_regex;
    RegexGetter m_regex_getter;

    FacesSpec   m_last_face;
    FaceGetter  m_face_getter;

    RegexHighlighter m_highlighter;
};

const HighlighterDesc dynamic_regex_desc = {
    "Parameters: <expr> <capture num>:<face> <capture num>:<face>...\n"
    "Evaluate expression at every redraw to gather a regex",
    {}
};
std::unique_ptr<Highlighter> create_dynamic_regex_highlighter(HighlighterParameters params, Highlighter*)
{
    if (params.size() < 2)
        throw runtime_error("wrong parameter count");

    Vector<std::pair<String, String>> faces;
    for (auto& spec : params.subrange(1))
    {
        auto colon = find(spec, ':');
        if (colon == spec.end())
            throw runtime_error("wrong face spec: '" + spec +
                                 "' expected <capture>:<facespec>");
        faces.emplace_back(String{spec.begin(), colon}, String{colon+1, spec.end()});
    }

    auto make_hl = [](auto& regex_getter, auto& face_getter) {
        return std::make_unique<DynamicRegexHighlighter<std::remove_cvref_t<decltype(regex_getter)>,
                                                        std::remove_cvref_t<decltype(face_getter)>>>(
            std::move(regex_getter), std::move(face_getter));
    };
    auto get_face = [faces=std::move(faces)](const Context& context, const Regex& regex){
        FacesSpec spec;
        for (auto& face : faces)
        {
            const int capture = str_to_int_ifp(face.first).value_or_compute([&] {
                return regex.named_capture_index(face.first);
            });
            if (capture < 0)
            {
                write_to_debug_buffer(format("Error while evaluating dynamic regex expression faces,"
                                             " {} is neither a capture index nor a capture name",
                                             face.first));
                return FacesSpec{};
            }
            spec.emplace_back(capture, face.second);
        }
        return spec;
    };

    CommandParser parser{params[0]};
    auto token = parser.read_token(true);
    if (token and parser.done() and token->type == Token::Type::OptionExpand and
        GlobalScope::instance().options()[token->content].is_of_type<Regex>())
    {
        auto get_regex = [option_name = token->content](const Context& context) {
            return context.options()[option_name].get<Regex>();
        };
        return make_hl(get_regex, get_face);
    }

    auto get_regex = [expr = params[0]](const Context& context){
        try
        {
            auto re = expand(expr, context);
            return re.empty() ? Regex{} : Regex{re};
        }
        catch (runtime_error& err)
        {
            write_to_debug_buffer(format("Error while evaluating dynamic regex expression: {}", err.what()));
            return Regex{};
        }
    };
    return make_hl(get_regex, get_face);
}

const HighlighterDesc line_desc = {
    "Parameters: <value string> <face>\n"
    "Highlight the line given by evaluating <value string> with <face>",
    {}
};
std::unique_ptr<Highlighter> create_line_highlighter(HighlighterParameters params, Highlighter*)
{
    if (params.size() != 2)
        throw runtime_error("wrong parameter count");

    auto func = [line_expr=params[0], facespec=params[1]]
                (HighlightContext context, DisplayBuffer& display_buffer, BufferRange)
    {
        LineCount line = -1;
        try
        {
            line = str_to_int_ifp(expand(line_expr, context.context)).value_or(0) - 1;
        }
        catch (runtime_error& err)
        {
            write_to_debug_buffer(
                format("Error evaluating highlight line expression: {}", err.what()));
        }

        if (line < 0)
            return;

        auto it = find_if(display_buffer.lines(),
                          [line](const DisplayLine& l)
                          { return l.range().begin.line == line; });
        if (it == display_buffer.lines().end())
            return;

        auto face = context.context.faces()[facespec];
        ColumnCount column = 0;
        for (auto& atom : *it)
        {
            column += atom.length();
            if (!atom.has_buffer_range())
                continue;

            kak_assert(atom.begin().line == line);
            apply_face(face)(atom);
        }
        const ColumnCount remaining = context.context.window().dimensions().column - column;
        if (remaining > 0)
            it->push_back({String{' ', remaining}, face});
    };

    return make_highlighter(std::move(func));
}

const HighlighterDesc column_desc = {
    "Parameters: <value string> <face>\n"
    "Highlight the column given by evaluating <value string> with <face>",
    {}
};
std::unique_ptr<Highlighter> create_column_highlighter(HighlighterParameters params, Highlighter*)
{
    if (params.size() != 2)
        throw runtime_error("wrong parameter count");

    auto func = [col_expr=params[0], facespec=params[1]]
                (HighlightContext context, DisplayBuffer& display_buffer, BufferRange)
    {
        ColumnCount column = -1;
        try
        {
            column = str_to_int_ifp(expand(col_expr, context.context)).value_or(0) - 1;
        }
        catch (runtime_error& err)
        {
            write_to_debug_buffer(
                format("Error evaluating highlight column expression: {}", err.what()));
        }

        if (column < 0)
            return;

        const auto face = context.context.faces()[facespec];
        if (column < context.setup.first_column or column >= context.setup.first_column + context.context.window().dimensions().column)
            return;

        column += context.setup.widget_columns;
        for (auto& line : display_buffer.lines())
        {
            auto remaining_col = column;
            bool found = false;
            for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
            {
                const auto atom_len = atom_it->length();
                if (remaining_col < atom_len)
                {
                    if (remaining_col > 0)
                        atom_it = ++line.split(atom_it, remaining_col);
                    if (atom_it->length() > 1)
                        atom_it = line.split(atom_it, 1_col);
                    atom_it->face = merge_faces(atom_it->face, face);
                    found = true;
                    break;
                }
                remaining_col -= atom_len;
            }
            if (found)
                continue;

            if (remaining_col > 0)
                line.push_back({String{' ', remaining_col}, Face{}});
            line.push_back({" ", face});
        }
    };

    return make_highlighter(std::move(func));
}

const HighlighterDesc wrap_desc = {
    "Parameters: [-word] [-indent] [-width <max_width>] [-marker <marker_text>]\n"
    "Wrap lines to window width",
    { {
        { "word",   { false, "wrap at word boundaries instead of codepoint boundaries" } },
        { "indent", { false, "preserve line indentation of the wrapped line" } },
        { "width",  { true, "wrap at the given column instead of the window's width" } },
        { "marker", { true, "insert the given text at the beginning of the wrapped line" } }, },
        ParameterDesc::Flags::None, 0, 0
    }
};
struct WrapHighlighter : Highlighter
{
    WrapHighlighter(ColumnCount max_width, bool word_wrap, bool preserve_indent, String marker)
        : Highlighter{HighlightPass::Wrap}, m_word_wrap{word_wrap},
          m_preserve_indent{preserve_indent}, m_max_width{max_width},
          m_marker{std::move(marker)} {}

    static constexpr StringView ms_id = "wrap";

    struct SplitPos{ ByteCount byte; ColumnCount column; };

    void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange) override
    {
        if (contains(context.disabled_ids, ms_id))
            return;

        const ColumnCount wrap_column = std::min(m_max_width, context.context.window().dimensions().column - context.setup.widget_columns);
        if (wrap_column <= 0)
            return;

        const Buffer& buffer = context.context.buffer();
        const auto& cursor = context.context.selections().main().cursor();
        const int tabstop = context.context.options()["tabstop"].get<int>();
        const LineCount win_height = context.context.window().dimensions().line;
        const ColumnCount marker_len = zero_if_greater(m_marker.column_length(), wrap_column);
        const Face face_marker = context.context.faces()["WrapMarker"];
        for (auto it = display_buffer.lines().begin();
             it != display_buffer.lines().end(); ++it)
        {
            const LineCount buf_line = it->range().begin.line;
            const ByteCount line_length = buffer[buf_line].length();
            const ColumnCount indent = m_preserve_indent ?
                zero_if_greater(line_indent(buffer, tabstop, buf_line), wrap_column) : 0_col;
            const ColumnCount prefix_len = std::max(marker_len, indent);

            auto pos = next_split_pos(buffer, wrap_column, prefix_len, tabstop, buf_line, {0, 0});
            if (pos.byte == line_length)
                continue;

            for (auto atom_it = it->begin();
                 pos.byte != line_length and atom_it != it->end(); )
            {
                const BufferCoord coord{buf_line, pos.byte};
                if (!atom_it->has_buffer_range() or
                    coord < atom_it->begin() or coord >= atom_it->end())
                {
                    ++atom_it;
                    continue;
                }

                auto& line = *it;

                if (coord > atom_it->begin())
                    atom_it = ++line.split(atom_it, coord);

                DisplayLine new_line{ AtomList{ atom_it, line.end() } };
                line.erase(atom_it, line.end());

                if (marker_len != 0)
                    new_line.insert(new_line.begin(), {m_marker, face_marker});
                if (indent > marker_len)
                {
                    auto it = new_line.insert(new_line.begin() + (marker_len > 0), {buffer, {coord, coord}});
                    it->replace(String{' ', indent - marker_len});
                }

                if (it+1 - display_buffer.lines().begin() == win_height)
                {
                    if (cursor >= new_line.range().begin) // strip first lines if cursor is not visible
                    {
                        display_buffer.lines().erase(display_buffer.lines().begin(), display_buffer.lines().begin()+1);
                        --it;
                    }
                    else
                    {
                        display_buffer.lines().erase(it+1, display_buffer.lines().end());
                        return;
                    }
                }
                it = display_buffer.lines().insert(it+1, new_line);

                pos = next_split_pos(buffer, wrap_column - prefix_len, prefix_len, tabstop, buf_line, pos);
                atom_it = it->begin();
            }
        }
    }

    void do_compute_display_setup(HighlightContext context, DisplaySetup& setup) const override
    {
        if (contains(context.disabled_ids, ms_id))
            return;

        const ColumnCount wrap_column = std::min(m_max_width, context.context.window().dimensions().column - setup.widget_columns);
        if (wrap_column <= 0)
            return;

        const Buffer& buffer = context.context.buffer();
        const auto& cursor = context.context.selections().main().cursor();
        const int tabstop = context.context.options()["tabstop"].get<int>();

        auto line_wrap_count = [&](LineCount line, ColumnCount prefix_len) {
            LineCount count = 0;
            const ByteCount line_length = buffer[line].length();
            SplitPos pos{0, 0};
            while (true)
            {
                pos = next_split_pos(buffer, wrap_column - (pos.byte == 0 ? 0_col : prefix_len),
                                     prefix_len, tabstop, line, pos);
                if (pos.byte == line_length)
                    break;
                ++count;
            }
            return count;
        };

        const auto win_height = context.context.window().dimensions().line;

        // Disable horizontal scrolling when using a WrapHighlighter
        setup.first_column = 0;
        setup.line_count = 0;
        setup.scroll_offset.column = 0;

        const ColumnCount marker_len = zero_if_greater(m_marker.column_length(), wrap_column);

        for (auto buf_line = setup.first_line, win_line = 0_line;
             win_line < win_height or buf_line <= cursor.line;
             ++buf_line, ++setup.line_count)
        {
            if (buf_line >= buffer.line_count())
                break;

            const ColumnCount indent = m_preserve_indent ?
                zero_if_greater(line_indent(buffer, tabstop, buf_line), wrap_column) : 0_col;
            const ColumnCount prefix_len = std::max(marker_len, indent);

            if (buf_line == cursor.line)
            {
                SplitPos pos{0, 0};
                for (LineCount count = 0; true; ++count)
                {
                    auto next_pos = next_split_pos(buffer, wrap_column - (pos.byte != 0 ? prefix_len : 0_col),
                                                   prefix_len, tabstop, buf_line, pos);
                    if (next_pos.byte > cursor.column)
                    {
                        setup.cursor_pos = DisplayCoord{
                            win_line + count,
                            get_column(buffer, tabstop, cursor) -
                            pos.column + (pos.byte != 0 ? indent : 0_col)
                        };
                        break;
                    }
                    pos = next_pos;
                }
            }
            const auto wrap_count = line_wrap_count(buf_line, prefix_len);
            win_line += wrap_count + 1;

            // scroll window to keep cursor visible, and update range as lines gets removed
            while (buf_line >= cursor.line and setup.first_line < cursor.line and
                   setup.cursor_pos.line + setup.scroll_offset.line >= win_height)
            {
                auto remove_count = 1 + line_wrap_count(setup.first_line, indent);
                ++setup.first_line;
                --setup.line_count;
                setup.cursor_pos.line -= std::min(win_height, remove_count);
                win_line -= remove_count;
                kak_assert(setup.cursor_pos.line >= 0);
            }
        }
    }

    void fill_unique_ids(Vector<StringView>& unique_ids) const override
    {
        unique_ids.push_back(ms_id);
    }

    SplitPos next_split_pos(const Buffer& buffer,  ColumnCount wrap_column, ColumnCount prefix_len,
                            int tabstop, LineCount line, SplitPos current) const
    {
        const ColumnCount target_column = current.column + wrap_column;
        StringView content = buffer[line];

        SplitPos pos = current;
        SplitPos last_word_boundary = {0, 0};
        SplitPos last_WORD_boundary = {0, 0};

        auto update_boundaries = [&](Codepoint cp) {
            if (not m_word_wrap)
                return;
            if (!is_word<Word>(cp))
                last_word_boundary = pos;
            if (!is_word<WORD>(cp))
                last_WORD_boundary = pos;
        };

        while (pos.byte < content.length() and pos.column < target_column)
        {
            if (content[pos.byte] == '\t')
            {
                const ColumnCount next_column = (pos.column / tabstop + 1) * tabstop;
                if (next_column > target_column and pos.byte != current.byte) // the target column was in the tab
                    break;
                pos.column = next_column;
                ++pos.byte;
                last_word_boundary = last_WORD_boundary = pos;
            }
            else
            {
                const char* it = &content[pos.byte];
                const Codepoint cp = utf8::read_codepoint(it, content.end());
                const ColumnCount width = codepoint_width(cp);
                if (pos.column + width > target_column and pos.byte != current.byte) // the target column was in the char
                {
                    update_boundaries(cp);
                    break;
                }
                pos.column += width;
                pos.byte = (int)(it - content.begin());
                update_boundaries(cp);
            }
        }

        if (m_word_wrap and pos.byte < content.length())
        {
            auto find_split_pos = [&](SplitPos start_pos, auto is_word) -> Optional<SplitPos> {
                if (start_pos.byte == 0)
                    return {};
                const char* it = &content[pos.byte];
                // split at current position if is a word boundary
                if (not is_word(utf8::codepoint(it, content.end()), {'_'}))
                    return pos;
                // split at last word boundary if the word is shorter than our wrapping width
                ColumnCount word_length = pos.column - start_pos.column;
                while (it != content.end() and word_length <= (wrap_column - prefix_len))
                {
                    const Codepoint cp = utf8::read_codepoint(it, content.end());
                    if (not is_word(cp, {'_'}))
                        return start_pos;
                    word_length += codepoint_width(cp);
                }
                return {};
            };
            if (auto split = find_split_pos(last_WORD_boundary, is_word<WORD>))
                return *split;
            if (auto split = find_split_pos(last_word_boundary, is_word<Word>))
                return *split;
        }

        return pos;
    }

    static ColumnCount line_indent(const Buffer& buffer, int tabstop, LineCount line)
    {
        StringView l = buffer[line];
        auto col = 0_byte;
        while (is_horizontal_blank(l[col]))
               ++col;
        return get_column(buffer, tabstop, {line, col});
    }

    static std::unique_ptr<Highlighter> create(HighlighterParameters params, Highlighter*)
    {
        ParametersParser parser(params, wrap_desc.params);
        ColumnCount max_width = parser.get_switch("width").map(str_to_int)
            .value_or(std::numeric_limits<int>::max());

        return std::make_unique<WrapHighlighter>(max_width, (bool)parser.get_switch("word"),
                                                 (bool)parser.get_switch("indent"),
                                                 parser.get_switch("marker").value_or("").str());
    }

    static ColumnCount zero_if_greater(ColumnCount val, ColumnCount max) { return val < max ? val : 0; };

    const bool m_word_wrap;
    const bool m_preserve_indent;
    const ColumnCount m_max_width;
    const String m_marker;
};

constexpr StringView WrapHighlighter::ms_id;

struct TabulationHighlighter : Highlighter
{
    TabulationHighlighter() : Highlighter{HighlightPass::Move} {}

    void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange) override
    {
        const ColumnCount tabstop = context.context.options()["tabstop"].get<int>();
        const auto& buffer = context.context.buffer();
        for (auto& line : display_buffer.lines())
        {
            for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
            {
                if (atom_it->type() != DisplayAtom::Range)
                    continue;

                auto begin = get_iterator(buffer, atom_it->begin());
                auto end = get_iterator(buffer, atom_it->end());
                for (BufferIterator it = begin; it != end; ++it)
                {
                    if (*it == '\t')
                    {
                        if (it != begin)
                            atom_it = ++line.split(atom_it, it.coord());
                        if (it+1 != end)
                            atom_it = line.split(atom_it, (it+1).coord());

                        const ColumnCount column = get_column(buffer, tabstop, it.coord());
                        const ColumnCount count = tabstop - (column % tabstop);
                        atom_it->replace(String{' ', count});
                        break;
                    }
                }
            }
        }
    }

    void do_compute_display_setup(HighlightContext context, DisplaySetup& setup) const override
    {
        auto& buffer = context.context.buffer();
        // Ensure that a cursor on a tab character makes the full tab character visible
        auto cursor = context.context.selections().main().cursor();
        if (buffer.byte_at(cursor) != '\t')
            return;

        const ColumnCount tabstop = context.context.options()["tabstop"].get<int>();
        const ColumnCount column = get_column(buffer, tabstop, cursor);
        const ColumnCount width = tabstop - (column % tabstop);
        const ColumnCount win_end = setup.first_column + context.context.window().dimensions().column - setup.widget_columns;
        const ColumnCount offset = std::max(column + width - win_end, 0_col);

        setup.first_column += offset;
        setup.cursor_pos.column -= offset;
    }
};

const HighlighterDesc show_whitespace_desc = {
    "Parameters: [-tab <separator>] [-tabpad <separator>] [-lf <separator>] [-spc <separator>] [-nbsp <separator>]\n"
    "Display whitespaces using symbols",
    { {
        { "tab",    { true, "replace tabulations with the given character" } },
        { "tabpad", { true, "append as many of the given character as is necessary to honor `tabstop`" } },
        { "spc",    { true, "replace spaces with the given character" } },
        { "lf",     { true, "replace line feeds with the given character" } },
        { "nbsp",   { true, "replace non-breakable spaces with the given character" } } },
        ParameterDesc::Flags::None, 0, 0
    }
};
struct ShowWhitespacesHighlighter : Highlighter
{
    ShowWhitespacesHighlighter(String tab, String tabpad, String spc, String lf, String nbsp)
      : Highlighter{HighlightPass::Move}, m_tab{std::move(tab)}, m_tabpad{std::move(tabpad)},
        m_spc{std::move(spc)}, m_lf{std::move(lf)}, m_nbsp{std::move(nbsp)}
    {}

    static std::unique_ptr<Highlighter> create(HighlighterParameters params, Highlighter*)
    {
        ParametersParser parser(params, show_whitespace_desc.params);

        auto get_param = [&](StringView param,  StringView fallback) {
            StringView value = parser.get_switch(param).value_or(fallback);
            if (value.char_length() != 1)
                throw runtime_error{format("-{} expects a single character parameter", param)};
            return value.str();
        };

        return std::make_unique<ShowWhitespacesHighlighter>(
            get_param("tab", "→"), get_param("tabpad", " "), get_param("spc", "·"),
            get_param("lf", "¬"), get_param("nbsp", "⍽"));
    }

private:
    void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange) override
    {
        const int tabstop = context.context.options()["tabstop"].get<int>();
        auto whitespaceface = context.context.faces()["Whitespace"];
        const auto& buffer = context.context.buffer();
        for (auto& line : display_buffer.lines())
        {
            for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
            {
                if (atom_it->type() != DisplayAtom::Range)
                    continue;

                auto begin = get_iterator(buffer, atom_it->begin());
                auto end = get_iterator(buffer, atom_it->end());
                for (BufferIterator it = begin; it != end; )
                {
                    auto coord = it.coord();
                    Codepoint cp = utf8::read_codepoint(it, end);
                    if (cp == '\t' or cp == ' ' or cp == '\n' or cp == 0xA0 or cp == 0x202F)
                    {
                        if (coord != begin.coord())
                            atom_it = ++line.split(atom_it, coord);
                        if (it != end)
                            atom_it = line.split(atom_it, it.coord());

                        if (cp == '\t')
                        {
                            const ColumnCount column = get_column(buffer, tabstop, coord);
                            const ColumnCount count = tabstop - (column % tabstop);
                            atom_it->replace(m_tab + String(m_tabpad[(CharCount)0], count - m_tab.column_length()));
                        }
                        else if (cp == ' ')
                            atom_it->replace(m_spc);
                        else if (cp == '\n')
                            atom_it->replace(m_lf);
                        else if (cp == 0xA0 or cp == 0x202F)
                            atom_it->replace(m_nbsp);
                        atom_it->face = merge_faces(atom_it->face, whitespaceface);
                        break;
                    }
                }
            }
        }
    }

    const String m_tab, m_tabpad, m_spc, m_lf, m_nbsp;
};

const HighlighterDesc line_numbers_desc = {
    "Parameters: [-relative] [-hlcursor] [-separators <separator|separator:cursor|cursor:up:down>] [-min-digits <cols>]\n"
    "Display line numbers",
    { {
        { "relative", { false, "show line numbers relative to the main cursor line" } },
        { "separator", { true, "string to separate the line numbers column from the rest of the buffer (default '|')" } },
        { "cursor-separator", { true, "identical to -separator but applies only to the line of the cursor (default is the same value passed to -separator)" } },
        { "min-digits", { true, "use at least the given number of columns to display line numbers (default 2)" } },
        { "hlcursor", { false, "highlight the cursor line with a separate face" } } },
        ParameterDesc::Flags::None, 0, 0
    }
};
struct LineNumbersHighlighter : Highlighter
{
    LineNumbersHighlighter(bool relative, bool hl_cursor_line, String separator, String cursor_separator, int min_digits)
      : Highlighter{HighlightPass::Move},
        m_relative{relative},
        m_hl_cursor_line{hl_cursor_line},
        m_separator{std::move(separator)},
        m_cursor_separator{std::move(cursor_separator)},
        m_min_digits{min_digits} {}

    static std::unique_ptr<Highlighter> create(HighlighterParameters params, Highlighter*)
    {
        ParametersParser parser(params, line_numbers_desc.params);

        StringView separator = parser.get_switch("separator").value_or("│");
        StringView cursor_separator = parser.get_switch("cursor-separator").value_or(separator);

        if (separator.length() > 10)
            throw runtime_error("separator length is limited to 10 bytes");

        if (cursor_separator.column_length() != separator.column_length())
            throw runtime_error("separator for active line should have the same length as 'separator'");

        int min_digits = parser.get_switch("min-digits").map(str_to_int).value_or(2);
        if (min_digits < 0)
            throw runtime_error("min digits must be positive");
        if (min_digits > 10)
            throw runtime_error("min digits is limited to 10");

        return std::make_unique<LineNumbersHighlighter>((bool)parser.get_switch("relative"), (bool)parser.get_switch("hlcursor"), separator.str(), cursor_separator.str(), min_digits);
    }

private:
    static constexpr StringView ms_id = "line-numbers";

    void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange) override
    {
        if (contains(context.disabled_ids, ms_id))
            return;

        const auto& faces = context.context.faces();
        const Face face = faces["LineNumbers"];
        const Face face_wrapped = faces["LineNumbersWrapped"];
        const Face face_absolute = faces["LineNumberCursor"];
        int digit_count = compute_digit_count(context.context);

        char format[16];
        format_to(format, "%{}d", digit_count);
        const int main_line = (int)context.context.selections().main().cursor().line + 1;
        int last_line = -1;
        for (auto& line : display_buffer.lines())
        {
            const int current_line = (int)line.range().begin.line + 1;
            const bool is_cursor_line = main_line == current_line;
            const int line_to_format = (m_relative and not is_cursor_line) ?
                                       current_line - main_line : current_line;
            char buffer[16];
            snprintf(buffer, 16, format, std::abs(line_to_format));
            const auto atom_face = last_line == current_line ? face_wrapped :
                ((m_hl_cursor_line and is_cursor_line) ? face_absolute : face);

            const auto& separator = is_cursor_line && last_line != current_line
                                    ? m_cursor_separator : m_separator;

            line.insert(line.begin(), {buffer, atom_face});
            line.insert(line.begin() + 1, {separator, face});

            last_line = current_line;
        }
    }

    void do_compute_display_setup(HighlightContext context, DisplaySetup& setup) const override
    {
        if (contains(context.disabled_ids, ms_id))
            return;

        ColumnCount width = compute_digit_count(context.context) + m_separator.column_length();
        setup.widget_columns += width;
    }

    void fill_unique_ids(Vector<StringView>& unique_ids) const override
    {
        unique_ids.push_back(ms_id);
    }

    int compute_digit_count(const Context& context) const
    {
        int digit_count = 0;
        LineCount last_line = context.buffer().line_count();
        for (LineCount c = last_line; c > 0; c /= 10)
            ++digit_count;
        return std::max(digit_count, m_min_digits);
    }

    const bool m_relative;
    const bool m_hl_cursor_line;
    const String m_separator;
    const String m_cursor_separator;
    const int m_min_digits;
};

constexpr StringView LineNumbersHighlighter::ms_id;


const HighlighterDesc show_matching_desc = {
    "Apply the MatchingChar face to the char matching the one under the cursor",
    {}
};
void show_matching_char(HighlightContext context, DisplayBuffer& display_buffer, BufferRange)
{
    const Face face = context.context.faces()["MatchingChar"];
    const auto& matching_pairs = context.context.options()["matching_pairs"].get<Vector<Codepoint, MemoryDomain::Options>>();
    const auto range = display_buffer.range();
    const auto& buffer = context.context.buffer();
    for (auto& sel : context.context.selections())
    {
        auto pos = sel.cursor();
        if (pos < range.begin or pos >= range.end)
            continue;

        Utf8Iterator it{buffer.iterator_at(pos), buffer};
        auto match = find(matching_pairs, *it);

        if (match == matching_pairs.end())
            continue;

        int level = 0;
        if (((match - matching_pairs.begin()) % 2) == 0)
        {
            const Codepoint opening = *match;
            const Codepoint closing = *(match+1);
            while (it.base().coord() <= range.end)
            {
                if (*it == opening)
                    ++level;
                else if (*it == closing and --level == 0)
                {
                    highlight_range(display_buffer, it.base().coord(), (it+1).base().coord(),
                                    false, apply_face(face));
                    break;
                }
                ++it;
            }
        }
        else if (pos > range.begin)
        {
            const Codepoint opening = *(match-1);
            const Codepoint closing = *match;
            while (true)
            {
                if (*it == closing)
                    ++level;
                else if (*it == opening and --level == 0)
                {
                    highlight_range(display_buffer, it.base().coord(), (it+1).base().coord(),
                                    false, apply_face(face));
                    break;
                }
                if (it.base().coord() <= range.begin)
                    break;
                --it;
            }
        }
    }
}

std::unique_ptr<Highlighter> create_matching_char_highlighter(HighlighterParameters params, Highlighter*)
{
    return make_highlighter(show_matching_char);
}

void highlight_selections(HighlightContext context, DisplayBuffer& display_buffer, BufferRange)
{
    const auto& buffer = context.context.buffer();
    const auto& faces = context.context.faces();
    const Face sel_faces[6] = {
            faces["PrimarySelection"], faces["SecondarySelection"],
            faces["PrimaryCursor"],    faces["SecondaryCursor"],
            faces["PrimaryCursorEol"], faces["SecondaryCursorEol"],
    };

    const auto& selections = context.context.selections();
    for (size_t i = 0; i < selections.size(); ++i)
    {
        auto& sel = selections[i];
        const bool forward = sel.anchor() <= sel.cursor();
        BufferCoord begin = forward ? sel.anchor() : buffer.char_next(sel.cursor());
        BufferCoord end   = forward ? (BufferCoord)sel.cursor() : buffer.char_next(sel.anchor());

        const bool primary = (i == selections.main_index());
        highlight_range(display_buffer, begin, end, false,
                        apply_face(sel_faces[primary ? 0 : 1]));
    }
    for (size_t i = 0; i < selections.size(); ++i)
    {
        auto& sel = selections[i];
        const BufferCoord coord = sel.cursor();
        const bool primary = (i == selections.main_index());
        const bool eol = buffer[coord.line].length() - 1 == coord.column;
        highlight_range(display_buffer, coord, buffer.char_next(coord), false,
                        apply_face(sel_faces[2 + (eol ? 2 : 0) + (primary ? 0 : 1)]));
    }
}

void expand_unprintable(HighlightContext context, DisplayBuffer& display_buffer, BufferRange)
{
    const auto& buffer = context.context.buffer();
    auto error = context.context.faces()["Error"];
    for (auto& line : display_buffer.lines())
    {
        for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
        {
            if (atom_it->type() == DisplayAtom::Range)
            {
                for (auto it  = get_iterator(buffer, atom_it->begin()),
                          end = get_iterator(buffer, atom_it->end()); it < end;)
                {
                    auto coord = it.coord();
                    Codepoint cp = utf8::read_codepoint(it, end);
                    if (cp != '\n' and not iswprint((wchar_t)cp))
                    {
                        if (coord != atom_it->begin())
                            atom_it = ++line.split(atom_it, coord);
                        if (it.coord() < atom_it->end())
                            atom_it = line.split(atom_it, it.coord());

                        atom_it->replace("�");
                        atom_it->face = error;
                        break;
                    }
                }
            }
        }
    }
}

static void update_line_specs_ifn(const Buffer& buffer, LineAndSpecList& line_flags)
{
    if (line_flags.prefix == buffer.timestamp())
        return;

    auto& lines = line_flags.list;

    auto modifs = compute_line_modifications(buffer, line_flags.prefix);
    auto ins_pos = lines.begin();
    for (auto it = lines.begin(); it != lines.end(); ++it)
    {
        auto& line = std::get<0>(*it); // that line is 1 based as it comes from user side
        auto modif_it = std::upper_bound(modifs.begin(), modifs.end(), line-1,
                                         [](const LineCount& l, const LineModification& c)
                                         { return l < c.old_line; });
        if (modif_it != modifs.begin())
        {
            auto& prev = *(modif_it-1);
            if (line-1 < prev.old_line + prev.num_removed)
                continue; // line removed

            line += prev.diff();
        }

        if (ins_pos != it)
            *ins_pos = std::move(*it);
        ++ins_pos;
    }
    lines.erase(ins_pos, lines.end());
    line_flags.prefix = buffer.timestamp();
}

void option_update(LineAndSpecList& opt, const Context& context)
{
    update_line_specs_ifn(context.buffer(), opt);
}

void option_list_postprocess(Vector<LineAndSpec, MemoryDomain::Options>& opt)
{
    std::sort(opt.begin(), opt.end(),
              [](auto& lhs, auto& rhs)
              { return std::get<0>(lhs) < std::get<0>(rhs); });
}

const HighlighterDesc flag_lines_desc = {
    "Parameters: <face> <option name>\n"
    "Display flags specified in the line-spec option <option name> with <face>",
    {}
};
struct FlagLinesHighlighter : Highlighter
{
    FlagLinesHighlighter(String option_name, String default_face)
        : Highlighter{HighlightPass::Move},
          m_option_name{std::move(option_name)},
          m_default_face{std::move(default_face)} {}

    static std::unique_ptr<Highlighter> create(HighlighterParameters params, Highlighter*)
    {
        if (params.size() != 2)
            throw runtime_error("wrong parameter count");

        const String& option_name = params[1];
        const String& default_face = params[0];

        // throw if wrong option type
        GlobalScope::instance().options()[option_name].get<LineAndSpecList>();

        return std::make_unique<FlagLinesHighlighter>(option_name, default_face);
    }

private:
    void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange) override
    {
        auto& line_flags = context.context.options()[m_option_name].get_mutable<LineAndSpecList>();
        const auto& buffer = context.context.buffer();
        update_line_specs_ifn(buffer, line_flags);

        auto def_face = context.context.faces()[m_default_face];
        Vector<DisplayLine> display_lines;
        auto& lines = line_flags.list;
        try
        {
            for (auto& line : lines)
            {
                display_lines.push_back(parse_display_line(std::get<1>(line), context.context.faces()));
                for (auto& atom : display_lines.back())
                    atom.face = merge_faces(def_face, atom.face);
            }
        }
        catch (runtime_error& err)
        {
            write_to_debug_buffer(format("Error while evaluating line flag: {}", err.what()));
            return;
        }

        ColumnCount width = 0;
        for (auto& l : display_lines)
             width = std::max(width, l.length());
        const DisplayAtom empty{String{' ', width}, def_face};
        for (auto& line : display_buffer.lines())
        {
            int line_num = (int)line.range().begin.line + 1;
            auto it = find_if(lines,
                              [&](const LineAndSpec& l)
                              { return std::get<0>(l) == line_num; });
            if (it == lines.end())
                line.insert(line.begin(), empty);
            else
            {
                DisplayLine& display_line = display_lines[it - lines.begin()];
                DisplayAtom padding_atom{String(' ', width - display_line.length()), def_face};
                auto it = std::copy(std::make_move_iterator(display_line.begin()),
                                    std::make_move_iterator(display_line.end()),
                                    std::inserter(line, line.begin()));

                if (padding_atom.length() != 0)
                    *it++ = std::move(padding_atom);
            }
        }
    }

    void do_compute_display_setup(HighlightContext context, DisplaySetup& setup) const override
    {
        auto& line_flags = context.context.options()[m_option_name].get_mutable<LineAndSpecList>();
        const auto& buffer = context.context.buffer();
        update_line_specs_ifn(buffer, line_flags);

        ColumnCount width = 0;
        try
        {
            for (auto& line : line_flags.list)
                width = std::max(parse_display_line(std::get<1>(line), context.context.faces()).length(), width);
        }
        catch (runtime_error& err)
        {
            write_to_debug_buffer(format("Error while evaluating line flag: {}", err.what()));
            return;
        }

        setup.widget_columns += width;
    }

    String m_option_name;
    String m_default_face;
};

bool is_empty(const InclusiveBufferRange& range)
{
    return range.last < BufferCoord{0,0};
}

String option_to_string(InclusiveBufferRange range)
{
    if (is_empty(range))
        return format("{}.{}+0", range.first.line+1, range.first.column+1);
    return format("{}.{},{}.{}",
                  range.first.line+1, range.first.column+1,
                  range.last.line+1, range.last.column+1);
}

InclusiveBufferRange option_from_string(Meta::Type<InclusiveBufferRange>, StringView str)
{
    auto sep = find_if(str, [](char c){ return c == ',' or c == '+'; });
    auto dot_beg = find(StringView{str.begin(), sep}, '.');
    auto dot_end = find(StringView{sep, str.end()}, '.');

    if (sep == str.end() or dot_beg == sep or
        (*sep == ',' and dot_end == str.end()))
        throw runtime_error(format("'{}' does not follow <line>.<column>,<line>.<column> or <line>.<column>+<len> format", str));

    const BufferCoord first{str_to_int({str.begin(), dot_beg}) - 1,
                            str_to_int({dot_beg+1, sep}) - 1};

    if (first.line < 0 or first.column < 0)
        throw runtime_error("coordinates elements should be >= 1");

    if (*sep == '+')
    {
        auto len = str_to_int({sep+1, str.end()});
        return {first, len == 0 ? BufferCoord{-1,-1} : BufferCoord{first.line, first.column + len - 1}};
    }

    const BufferCoord last{str_to_int({sep+1, dot_end}) - 1, str_to_int({dot_end+1, str.end()}) - 1};
    if (last.line < 0 or last.column < 0)
        throw runtime_error("coordinates elements should be >= 1");

    return { std::min(first, last), std::max(first, last) };
}

template<typename OptionType, typename DerivedType, HighlightPass pass = HighlightPass::Colorize>
struct OptionBasedHighlighter : Highlighter
{
    OptionBasedHighlighter(String option_name)
        : Highlighter{pass}
        , m_option_name{std::move(option_name)} {}

    static std::unique_ptr<Highlighter> create(HighlighterParameters params, Highlighter*)
    {
        if (params.size() != 1)
            throw runtime_error("wrong parameter count");

        const String& option_name = params[0];
        // throw if wrong option type
        GlobalScope::instance().options()[option_name].get<OptionType>();

        return std::make_unique<DerivedType>(option_name);
    }

    OptionType& get_option(const HighlightContext& context) const
    {
        return context.context.options()[m_option_name].template get_mutable<OptionType>();
    }

private:
    const String m_option_name;
};

BufferCoord& get_first(RangeAndString& r) { return std::get<0>(r).first; }
BufferCoord& get_last(RangeAndString& r) { return std::get<0>(r).last; }

bool option_element_compare(RangeAndString const& lhs, RangeAndString const& rhs)
{
    return std::get<0>(lhs).first == std::get<0>(rhs).first ?
        std::get<0>(lhs).last < std::get<0>(rhs).last
      : std::get<0>(lhs).first < std::get<0>(rhs).first;
}

void option_list_postprocess(Vector<RangeAndString, MemoryDomain::Options>& opt)
{
    std::sort(opt.begin(), opt.end(), option_element_compare);
}

void option_update(RangeAndStringList& opt, const Context& context)
{
    update_ranges(context.buffer(), opt.prefix, opt.list);
}

bool option_add_from_strings(Vector<RangeAndString, MemoryDomain::Options>& opt, ConstArrayView<String> strs)
{
    auto vec = option_from_strings(Meta::Type<Vector<RangeAndString, MemoryDomain::Options>>{}, strs);
    if (vec.empty())
        return false;
    auto middle = opt.insert(opt.end(),
                             std::make_move_iterator(vec.begin()),
                             std::make_move_iterator(vec.end()));
    std::sort(middle, opt.end(), option_element_compare);
    std::inplace_merge(opt.begin(), middle, opt.end(), option_element_compare);
    return true;
}

const HighlighterDesc ranges_desc = {
    "Parameters: <option name>\n"
    "Use the range-specs option given as parameter to highlight buffer\n"
    "each spec is interpreted as a face to apply to the range",
    {}
};
struct RangesHighlighter : OptionBasedHighlighter<RangeAndStringList, RangesHighlighter>
{
    using RangesHighlighter::OptionBasedHighlighter::OptionBasedHighlighter;
private:
    void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange) override
    {
        auto& buffer = context.context.buffer();
        auto& range_and_faces = get_option(context);
        update_ranges(buffer, range_and_faces.prefix, range_and_faces.list);

        for (auto& [range, face] : range_and_faces.list)
        {
            try
            {
                if (buffer.is_valid(range.first) and (buffer.is_valid(range.last) and not buffer.is_end(range.last)))
                    highlight_range(display_buffer, range.first, buffer.char_next(range.last), false,
                                    apply_face(context.context.faces()[face]));
            }
            catch (runtime_error&)
            {}
        }
    }
};

const HighlighterDesc replace_ranges_desc = {
    "Parameters: <option name>\n"
    "Use the range-specs option given as parameter to highlight buffer\n"
    "each spec is interpreted as a display line to display in place of the range",
    {}
};
struct ReplaceRangesHighlighter : OptionBasedHighlighter<RangeAndStringList, ReplaceRangesHighlighter, HighlightPass::Move>
{
    using ReplaceRangesHighlighter::OptionBasedHighlighter::OptionBasedHighlighter;
private:
    static bool is_valid(Buffer& buffer, BufferCoord c)
    {
        return c.line >= 0 and c.column >= 0 and c.line < buffer.line_count() and c.column <= buffer[c.line].length();
    }

    static bool is_fully_selected(const SelectionList& sels, const InclusiveBufferRange& range)
    {
        auto it = std::lower_bound(sels.begin(), sels.end(), range.first, [](const Selection& s, const BufferCoord& c) { return s.max() < c; });
        if (it == sels.end())
            return true;
        return it->min() > range.last or (it->min() <= range.first and it->max() >= range.last);
    }

    void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange) override
    {
        auto& buffer = context.context.buffer();
        auto& sels = context.context.selections();
        auto& range_and_faces = get_option(context);
        update_ranges(buffer, range_and_faces.prefix, range_and_faces.list);

        for (auto& [range, spec] : range_and_faces.list)
        {
            try
            {
                if (!is_valid(buffer, range.first) or (!is_empty(range) and !is_valid(buffer, range.last)) or !is_fully_selected(sels, range))
                    continue;
                auto replacement = parse_display_line(spec, context.context.faces());
                auto end = is_empty(range) ? range.first : buffer.char_next(range.last);
                replace_range(display_buffer, range.first, end,
                              [&, range=BufferRange{range.first, end}]
                              (DisplayLine& line, DisplayLine::iterator pos){
                                  for (auto& atom : replacement)
                                  {
                                      atom.replace(range);
                                      pos = ++line.insert(pos, std::move(atom));
                                  }
                              });
            }
            catch (runtime_error&)
            {}
        }
    }

    void do_compute_display_setup(HighlightContext context, DisplaySetup& setup) const override
    {
        auto& buffer = context.context.buffer();
        auto& sels = context.context.selections();
        auto& range_and_faces = get_option(context);
        const int tabstop = context.context.options()["tabstop"].get<int>();
        update_ranges(buffer, range_and_faces.prefix, range_and_faces.list);

        for (auto& [range, spec] : range_and_faces.list)
        {
            if (!is_valid(buffer, range.first) or (!is_empty(range) and !is_valid(buffer, range.last)) or !is_fully_selected(sels, range))
                continue;

            auto last = is_empty(range) ? range.first : range.last;
            if (range.first.line < setup.first_line and last.line >= setup.first_line)
                setup.first_line = range.first.line;

            const auto& cursor = context.context.selections().main().cursor();
            if (cursor.line == last.line and cursor.column >= last.column)
            {
                auto first_column = get_column(buffer, tabstop, range.first);
                auto last_column = get_column(buffer, tabstop, last);
                auto replacement = parse_display_line(spec, context.context.faces());
                auto cursor_move = replacement.length() - ((range.first.line == last.line) ? last_column - first_column : last_column);
                setup.cursor_pos.line -= last.line - range.first.line;
                setup.cursor_pos.column += cursor_move;
            }

            if (last.line >= setup.first_line and
                range.first.line <= setup.first_line + setup.line_count and
                range.first.line != last.line)
            {
                auto removed_count = last.line - range.first.line;
                setup.line_count += removed_count;
            }
        }
    }
};

HighlightPass parse_passes(StringView str)
{
    HighlightPass passes{};
    for (auto pass : str | split<StringView>('|'))
    {
        if (pass == "colorize")
            passes |= HighlightPass::Colorize;
        else if (pass == "move")
            passes |= HighlightPass::Move;
        else if (pass == "wrap")
            passes |= HighlightPass::Wrap;
        else
            throw runtime_error{format("invalid highlight pass: {}", pass)};
    }
    if (passes == HighlightPass{})
        throw runtime_error{"no passes specified"};

    return passes;
}

const HighlighterDesc higlighter_group_desc = {
    "Parameters: [-passes <passes>]\n"
    "Creates a group that can contain other highlighters",
    { {
        { "passes", { true, "flags(colorize|move|wrap) "
                            "kind of highlighters can be put in the group "
                            "(default colorize)" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart, 0, 0
    }
};
std::unique_ptr<Highlighter> create_highlighter_group(HighlighterParameters params, Highlighter*)
{
    ParametersParser parser{params, higlighter_group_desc.params};
    HighlightPass passes = parse_passes(parser.get_switch("passes").value_or("colorize"));

    return std::make_unique<HighlighterGroup>(passes);
}

const HighlighterDesc ref_desc = {
    "Parameters: [-passes <passes>] <path>\n"
    "Reference the highlighter at <path> in shared highlighters",
    { {
        { "passes", { true, "flags(colorize|move|wrap) "
                            "kind of highlighters that can be referenced "
                            "(default colorize)" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart, 1, 1
    }
};
struct ReferenceHighlighter : Highlighter
{
    ReferenceHighlighter(HighlightPass passes, String name)
        : Highlighter{passes}, m_name{std::move(name)} {}

    static std::unique_ptr<Highlighter> create(HighlighterParameters params, Highlighter*)
    {
        ParametersParser parser{params, ref_desc.params};
        HighlightPass passes = parse_passes(parser.get_switch("passes").value_or("colorize"));
        return std::make_unique<ReferenceHighlighter>(passes, parser[0]);
    }

private:
    void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange range) override
    {
        static Vector<std::pair<StringView, BufferRange>> running_refs;
        const std::pair<StringView, BufferRange> desc{m_name, range};
        if (contains(running_refs, desc))
            return write_to_debug_buffer(format("highlighting recursion detected with ref to {}", m_name));

        running_refs.push_back(desc);
        auto pop_desc = on_scope_end([] { running_refs.pop_back(); });

        try
        {
            SharedHighlighters::instance().get_child(m_name).highlight(context, display_buffer, range);
        }
        catch (child_not_found&)
        {}
    }

    void do_compute_display_setup(HighlightContext context, DisplaySetup& setup) const override
    {
        try
        {
            SharedHighlighters::instance().get_child(m_name).compute_display_setup(context, setup);
        }
        catch (child_not_found&)
        {}
    }

    const String m_name;
};

struct RegexMatch
{
    LineCount line;
    ByteCount begin;
    ByteCount end;
    uint16_t capture_pos;
    uint16_t capture_len;

    BufferCoord begin_coord() const { return { line, begin }; }
    BufferCoord end_coord() const { return { line, end }; }
    bool empty() const { return begin == end; }

    StringView capture(const Buffer& buffer) const
    {
        if (capture_len == 0)
            return {};
        return buffer[line].substr(begin + capture_pos, ByteCount{capture_len});
    }
};

using RegexMatchList = Vector<RegexMatch, MemoryDomain::Regions>;

struct RegionMatches : UseMemoryDomain<MemoryDomain::Highlight>
{
    RegexMatchList begin_matches;
    RegexMatchList end_matches;
    RegexMatchList recurse_matches;
};

const HighlighterDesc default_region_desc = {
    "Parameters: <delegate_type> <delegate_params>...\n"
    "Define the default region of a regions highlighter",
    {}
};
const HighlighterDesc region_desc = {
    "Parameters:  [-match-capture] [-recurse <recurse>] <opening> <closing> <type> <params>...\n"
    "Define a region for a regions highlighter, and apply the given delegate\n"
    "highlighter as defined by <type> and eventual <params>...\n"
    "The region starts at <begin> match and ends at the first <end>",
    { {
        { "match-capture", { false, "only consider region ending/recurse delimiters whose first capture group match the region beginning delimiter" } },
        { "recurse",       { true, "make the region end on the first ending delimiter that does not close the given parameter" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart | ParameterDesc::Flags::IgnoreUnknownSwitches,
        3
    }
};
const HighlighterDesc regions_desc = {
    "Holds child region highlighters and segments the buffer in ranges based on those regions\n"
    "definitions. The regions highlighter finds the next region to start by finding which\n"
    "of its child region has the leftmost starting point from current position. In between\n"
    "regions, the default-region child highlighter is applied (if such a child exists)",
    {}
};
struct RegionsHighlighter : public Highlighter
{
public:
    RegionsHighlighter()
        : Highlighter{HighlightPass::Colorize} {}

    void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange range) override
    {
        if (m_regions.empty())
            return;

        auto display_range = display_buffer.range();
        const auto& buffer = context.context.buffer();
        auto& regions = get_regions_for_range(buffer, range);

        auto begin = std::lower_bound(regions.begin(), regions.end(), display_range.begin,
                                      [](const Region& r, BufferCoord c) { return r.end < c; });
        auto end = std::lower_bound(begin, regions.end(), display_range.end,
                                    [](const Region& r, BufferCoord c) { return r.begin < c; });
        auto correct = [&](BufferCoord c) -> BufferCoord {
            if (not buffer.is_end(c) and buffer[c.line].length() == c.column)
                return {c.line+1, 0};
            return c;
        };

        auto default_region_it = m_regions.find(m_default_region);
        const bool apply_default = default_region_it != m_regions.end();

        auto last_begin = (begin == regions.begin()) ?
                             range.begin : (begin-1)->end;
        kak_assert(begin <= end);
        for (; begin != end; ++begin)
        {
            if (apply_default and last_begin < begin->begin)
                apply_highlighter(context, display_buffer,
                                  correct(last_begin), correct(begin->begin),
                                  *default_region_it->value);

            auto it = m_regions.find(begin->region);
            if (it == m_regions.end())
                continue;
            apply_highlighter(context, display_buffer,
                              correct(begin->begin), correct(begin->end),
                              *it->value);
            last_begin = begin->end;
        }
        if (apply_default and last_begin < display_range.end)
            apply_highlighter(context, display_buffer,
                              correct(last_begin), range.end,
                              *default_region_it->value);
    }

    bool has_children() const override { return true; }

    Highlighter& get_child(StringView path) override
    {
        auto sep_it = find(path, '/');
        StringView id(path.begin(), sep_it);
        auto it = m_regions.find(id);
        if (it == m_regions.end())
            throw child_not_found(format("no such id: {}", id));
        if (sep_it == path.end())
            return *it->value;
        else
            return it->value->get_child({sep_it+1, path.end()});
    }

    void add_child(String name, std::unique_ptr<Highlighter>&& hl, bool override) override
    {
        if (not dynamic_cast<RegionHighlighter*>(hl.get()))
            throw runtime_error{"only region highlighter can be added as child of a regions highlighter"};
        auto it = m_regions.find(name);
        if (not override and it != m_regions.end())
            throw runtime_error{format("duplicate id: '{}'", name)};

        std::unique_ptr<RegionHighlighter> region_hl{dynamic_cast<RegionHighlighter*>(hl.release())};
        if (region_hl->is_default())
        {
            if (not m_default_region.empty())
                throw runtime_error{"default region already defined"};
            m_default_region = name;
        }

        if (it != m_regions.end())
            it->value = std::move(region_hl);
        else
            m_regions.insert({std::move(name), std::move(region_hl)});
        ++m_regions_timestamp;
    }

    void remove_child(StringView id) override
    {
        if (id == m_default_region)
            m_default_region = String{};

        m_regions.remove(id);
        ++m_regions_timestamp;
    }

    Completions complete_child(StringView path, ByteCount cursor_pos, bool group) const override
    {
        auto sep_it = find(path, '/');
        if (sep_it != path.end())
        {
            ByteCount offset = sep_it+1 - path.begin();
            Highlighter& hl = const_cast<RegionsHighlighter*>(this)->get_child({path.begin(), sep_it});
            return offset_pos(hl.complete_child(path.substr(offset), cursor_pos - offset, group), offset);
        }

        auto container = m_regions | transform(&decltype(m_regions)::Item::key);
        auto completions_flags = group ? Completions::Flags::None : Completions::Flags::Menu;
        return { 0, 0, complete(path, cursor_pos, container), completions_flags };
    }

    static std::unique_ptr<Highlighter> create(HighlighterParameters params, Highlighter*)
    {
        if (not params.empty())
            throw runtime_error{"unexpected parameters"};
        return std::make_unique<RegionsHighlighter>();
    }

    static bool is_regions(Highlighter* parent)
    {
        if (dynamic_cast<RegionsHighlighter*>(parent))
            return true;
        if (auto* region = dynamic_cast<RegionHighlighter*>(parent))
            return is_regions(&region->delegate());
        return false;
    }

    static std::unique_ptr<Highlighter> create_region(HighlighterParameters params, Highlighter* parent)
    {
        if (not is_regions(parent))
            throw runtime_error{"region highlighter can only be added to a regions parent"};

        ParametersParser parser{params, region_desc.params};

        const bool match_capture = (bool)parser.get_switch("match-capture");
        if (parser[0].empty() or parser[1].empty())
            throw runtime_error("begin and end must not be empty");

        const auto& type = parser[2];
        auto& registry = HighlighterRegistry::instance();
        auto it = registry.find(type);
        if (it == registry.end())
            throw runtime_error(format("no such highlighter type: '{}'", type));

        // validate regexes, TODO: less costly
        Regex{parser[0]};
        Regex{parser[1]};
        if (auto recurse_switch = parser.get_switch("recurse"))
            Regex{*recurse_switch};

        auto delegate = it->value.factory(parser.positionals_from(3), nullptr);
        return std::make_unique<RegionHighlighter>(std::move(delegate), parser[0], parser[1], parser.get_switch("recurse").value_or("").str(), match_capture);
    }

    static std::unique_ptr<Highlighter> create_default_region(HighlighterParameters params, Highlighter* parent)
    {
        if (not is_regions(parent))
            throw runtime_error{"default-region highlighter can only be added to a regions parent"};

        static const ParameterDesc param_desc{ {}, ParameterDesc::Flags::SwitchesOnlyAtStart, 1 };
        ParametersParser parser{params, param_desc};

        auto delegate = HighlighterRegistry::instance()[parser[0]].factory(parser.positionals_from(1), nullptr);
        return std::make_unique<RegionHighlighter>(std::move(delegate));
    }

private:
    struct Region
    {
        BufferCoord begin;
        BufferCoord end;
        StringView region;
    };
    using RegionList = Vector<Region, MemoryDomain::Highlight>;

    struct RegexKey
    {
        StringView regex;
        bool match_captures;

        friend size_t hash_value(const RegexKey& key) { return hash_values(key.regex, key.match_captures); }
        friend bool operator==(const RegexKey&, const RegexKey&) = default;
    };

    struct Cache
    {
        size_t buffer_timestamp = 0;
        size_t regions_timestamp = 0;
        LineRangeSet ranges;
        HashMap<RegexKey, RegexMatchList> matches;
        HashMap<BufferRange, RegionList, MemoryDomain::Highlight> regions;
    };

    struct RegionHighlighter : public Highlighter
    {
        RegionHighlighter(std::unique_ptr<Highlighter>&& delegate,
                          String begin, String end, String recurse,
                          bool match_capture)
            : Highlighter{delegate->passes()},
              m_delegate{std::move(delegate)},
              m_begin{std::move(begin)}, m_end{std::move(end)}, m_recurse{std::move(recurse)},
              m_match_capture{match_capture}
       {
       }

        RegionHighlighter(std::unique_ptr<Highlighter>&& delegate)
            : Highlighter{delegate->passes()}, m_delegate{std::move(delegate)}, m_default{true}
       {
       }

        bool has_children() const override
        {
            return m_delegate->has_children();
        }

        Highlighter& get_child(StringView path) override
        {
            return m_delegate->get_child(path);
        }

        void add_child(String name, std::unique_ptr<Highlighter>&& hl, bool override) override
        {
            return m_delegate->add_child(name, std::move(hl), override);
        }

        void remove_child(StringView id) override
        {
            return m_delegate->remove_child(id);
        }

        Completions complete_child(StringView path, ByteCount cursor_pos, bool group) const override
        {
            return m_delegate->complete_child(path, cursor_pos, group);
        }

        void fill_unique_ids(Vector<StringView>& unique_ids) const override
        {
            return m_delegate->fill_unique_ids(unique_ids);
        }

        void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange range) override
        {
            return m_delegate->highlight(context, display_buffer, range);
        }


        bool match_capture() const { return m_match_capture; }
        bool is_default() const { return m_default; }

        Highlighter& delegate() { return *m_delegate; }

    // private:
        std::unique_ptr<Highlighter> m_delegate;

        String m_begin;
        String m_end;
        String m_recurse;
        bool  m_match_capture = false;
        bool  m_default = false;
    };


    using RegionAndMatch = std::pair<size_t, RegexMatchList::const_iterator>;

    static bool compare_to_begin(const RegexMatch& lhs, BufferCoord rhs)
    {
        return lhs.begin_coord() < rhs;
    }

    RegexMatchList::const_iterator find_matching_end(const Buffer& buffer, BufferCoord beg_pos, const RegexMatchList& end_matches, const RegexMatchList& recurse_matches, Optional<StringView> capture) const
    {
        auto end_it = end_matches.begin();
        auto rec_it = recurse_matches.begin();
        int recurse_level = 0;
        while (true)
        {
            end_it = std::lower_bound(end_it, end_matches.end(), beg_pos,
                                      compare_to_begin);
            rec_it = std::lower_bound(rec_it, recurse_matches.end(), beg_pos,
                                      compare_to_begin);

            if (end_it == end_matches.end())
                return end_it;

            while (rec_it != recurse_matches.end() and
                   rec_it->end_coord() <= end_it->end_coord())
            {
                if (not capture or rec_it->capture(buffer) == *capture)
                    ++recurse_level;
                ++rec_it;
            }

            if (not capture or *capture == end_it->capture(buffer))
            {
                if (recurse_level == 0)
                    return end_it;
                --recurse_level;
            }

            if (beg_pos != end_it->end_coord())
                beg_pos = end_it->end_coord();
            ++end_it;
        }
    }

    // find the begin closest to pos in all matches
    Optional<RegionAndMatch> find_next_begin(const Cache& cache, BufferCoord pos) const
    {
        Optional<RegionAndMatch> res;

        for (size_t i = 0; i < m_regions.size(); ++i)
        {
            const auto& [key, region] = m_regions.item(i);
            if (region->is_default())
                continue;

            const auto& matches = cache.matches.get(RegexKey{region->m_begin, region->match_capture()});
            auto it = std::lower_bound(matches.begin(), matches.end(), pos, compare_to_begin);
            if (it != matches.end() and (not res or it->begin_coord() < res->second->begin_coord()))
                res = RegionAndMatch{i, it};
        }
        return res;
    }

    void add_regex(const String& str, bool captures)
    {
        const RegexKey key{str, captures};
        if (str.empty() or m_regexes.contains(key))
            return;

        auto flags = RegexCompileFlags::Optimize;
        if (not captures)
            flags |= RegexCompileFlags::NoSubs;

        m_regexes.insert({key, Regex{str, flags}});
    }

    class MatchAdder
    {
    public:
        MatchAdder(RegionsHighlighter& region, const Buffer& buffer, Cache& cache) : m_buffer(buffer)
        {
            for (auto& [key, regex] : region.m_regexes)
                cache.matches[key];
            for (auto& [key, regex] : region.m_regexes)
                m_matchers.push_back(Matcher{cache.matches.get(key), regex});
        }

        ~MatchAdder()
        {
            // Move new matches into position.
            for (auto& [matches, regex, pivot, vm] : m_matchers)
                std::inplace_merge(matches.begin(), matches.begin() + pivot, matches.end(),
                                   [](const auto& lhs, const auto& rhs) { return lhs.line < rhs.line; });
        }

        void add(LineRange range)
        {
            for (auto line = range.begin; line < range.end; ++line)
            {
                const StringView l = m_buffer[line];
                const auto flags = RegexExecFlags::NotEndOfLine; // buffer line already ends with \n

                for (auto& [matches, regex, pivot, vm] : m_matchers)
                {
                    auto extra_flags = RegexExecFlags::None;
                    auto pos = l.begin();
                    while (vm.exec(pos, l.end(), l.begin(), l.end(), flags | extra_flags))
                    {
                        ConstArrayView<const char*> captures = vm.captures();
                        const bool with_capture = regex.mark_count() > 0 and captures[2] != nullptr and
                                                  captures[1] - captures[0] < std::numeric_limits<uint16_t>::max();
                        matches.push_back({
                            line,
                            (int)(captures[0] - l.begin()),
                            (int)(captures[1] - l.begin()),
                            (uint16_t)(with_capture ? captures[2] - captures[0] : 0),
                            (uint16_t)(with_capture ? captures[3] - captures[2] : 0)
                        });
                        pos = captures[1];

                        extra_flags = (captures[0] == captures[1]) ? RegexExecFlags::NotInitialNull : RegexExecFlags::None;
                    }
                }
            }
        }

    private:
        struct Matcher
        {
            RegexMatchList& matches;
            const Regex& regex;
            size_t pivot = matches.size();
            ThreadedRegexVM<const char*, RegexMode::Forward | RegexMode::Search> vm{*regex.impl()};
        };

        const Buffer& m_buffer;
        Vector<Matcher> m_matchers;
    };

    void update_changed_lines(const Buffer& buffer, ConstArrayView<LineModification> modifs, Cache& cache)
    {
        for (auto& [key, matches] : cache.matches)
        {
            // remove out of date matches and update line for others
            auto ins_pos = matches.begin();
            for (auto it = ins_pos; it != matches.end(); ++it)
            {
                auto modif_it = std::upper_bound(modifs.begin(), modifs.end(), it->line,
                                                 [](const LineCount& l, const LineModification& c)
                                                 { return l < c.old_line; });

                if (modif_it != modifs.begin())
                {
                    auto& prev = *(modif_it-1);
                    if (it->line < prev.old_line + prev.num_removed)
                        continue; // match removed

                    it->line += prev.diff();
                }

                kak_assert(buffer.is_valid(it->begin_coord()) or
                           buffer[it->line].length() == it->begin);
                kak_assert(buffer.is_valid(it->end_coord()) or
                           buffer[it->line].length() == it->end);

                if (ins_pos != it)
                    *ins_pos = std::move(*it);
                ++ins_pos;
            }
            matches.erase(ins_pos, matches.end());
        }
    }

    bool update_matches(Cache& cache, const Buffer& buffer, LineRange range)
    {
        const size_t buffer_timestamp = buffer.timestamp();
        if (cache.buffer_timestamp == 0 or
            cache.regions_timestamp != m_regions_timestamp)
        {
            m_regexes.clear();
            cache.matches.clear();
            for (auto& [key, region] : m_regions)
            {
                add_regex(region->m_begin, region->match_capture());
                add_regex(region->m_end, region->match_capture());
                add_regex(region->m_recurse, region->match_capture());
            }

            MatchAdder{*this, buffer, cache}.add(range);
            cache.ranges.reset(range);
            cache.buffer_timestamp = buffer_timestamp;
            cache.regions_timestamp = m_regions_timestamp;
            return true;
        }
        else
        {
            bool modified = false;
            if (cache.buffer_timestamp != buffer_timestamp)
            {
                auto modifs = compute_line_modifications(buffer, cache.buffer_timestamp);
                update_changed_lines(buffer, modifs, cache);
                cache.ranges.update(modifs);
                cache.buffer_timestamp = buffer_timestamp;
                modified = true;
            }

            MatchAdder matches{*this, buffer, cache};
            cache.ranges.add_range(range, [&](const LineRange& range) {
                if (range.begin == range.end)
                    return;
                matches.add(range);
                modified = true;
            });
            return modified;
        }
    }

    const RegionList& get_regions_for_range(const Buffer& buffer, BufferRange range)
    {
        Cache& cache = m_cache.get(buffer);
        if (update_matches(cache, buffer, {range.begin.line, std::min(buffer.line_count(), range.end.line + 1)}))
            cache.regions.clear();

        auto it = cache.regions.find(range);
        if (it != cache.regions.end())
            return it->value;

        RegionList& regions = cache.regions[range];
        RegexMatchList empty_matches{};

        for (auto begin = find_next_begin(cache, range.begin); begin; )
        {
            auto& [index, beg_it] = *begin;
            auto& [name, region] = m_regions.item(index);
            auto& end_matches = cache.matches.get(RegexKey{region->m_end, region->match_capture()});
            auto& recurse_matches = region->m_recurse.empty() ?
                empty_matches : cache.matches.get(RegexKey{region->m_recurse, region->match_capture()});

            auto end_it = find_matching_end(buffer, beg_it->end_coord(), end_matches, recurse_matches,
                                            region->match_capture() ? beg_it->capture(buffer) : Optional<StringView>{});

            if (end_it == end_matches.end() or end_it->end_coord() >= range.end) // region continue past range end
            {
                auto begin_coord = beg_it->begin_coord();
                if (begin_coord < range.end)
                    regions.push_back({begin_coord, range.end, name});
                break;
            }

            auto end_coord = end_it->end_coord();
            regions.push_back({beg_it->begin_coord(), end_coord, name});

            // With empty begin and end matches (for example if the regexes
            // are /"\K/ and /(?=")/), that case can happen, and would
            // result in an infinite loop.
            if (end_coord == beg_it->begin_coord())
            {
                kak_assert(beg_it->empty() and end_it->empty());
                ++end_coord.column;
            }
            begin = find_next_begin(cache, end_coord);
        }
        return regions;
    }

    HashMap<String, std::unique_ptr<RegionHighlighter>, MemoryDomain::Highlight> m_regions;
    HashMap<RegexKey, Regex> m_regexes;
    String m_default_region;

    size_t m_regions_timestamp = 0;
    BufferSideCache<Cache> m_cache;
};

void setup_builtin_highlighters(HighlighterGroup& group)
{
    group.add_child("tabulations"_str, std::make_unique<TabulationHighlighter>());
    group.add_child("unprintable"_str, make_highlighter(expand_unprintable));
    group.add_child("selections"_str,  make_highlighter(highlight_selections));
}

void register_highlighters()
{
    HighlighterRegistry& registry = HighlighterRegistry::instance();

    registry.insert({
        "column",
        { create_column_highlighter, &column_desc } });
    registry.insert({
        "default-region",
        { RegionsHighlighter::create_default_region, &default_region_desc } });
    registry.insert({
        "dynregex",
        { create_dynamic_regex_highlighter, &dynamic_regex_desc } });
    registry.insert({
        "fill",
        { create_fill_highlighter, &fill_desc } });
    registry.insert({
        "flag-lines",
        { FlagLinesHighlighter::create, &flag_lines_desc } });
    registry.insert({
        "group",
        { create_highlighter_group, &higlighter_group_desc } });
    registry.insert({
        "line",
        { create_line_highlighter, &line_desc } });
    registry.insert({
        "number-lines",
        { LineNumbersHighlighter::create, &line_numbers_desc } });
    registry.insert({
        "ranges",
        { RangesHighlighter::create, &ranges_desc } });
    registry.insert({
        "ref",
        { ReferenceHighlighter::create, &ref_desc } });
    registry.insert({
        "regex",
        { RegexHighlighter::create, &regex_desc } });
    registry.insert({
        "region",
        { RegionsHighlighter::create_region, &region_desc } });
    registry.insert({
        "regions",
        { RegionsHighlighter::create, &regions_desc } });
    registry.insert({
        "replace-ranges",
        { ReplaceRangesHighlighter::create, &replace_ranges_desc } });
    registry.insert({
        "show-matching",
        { create_matching_char_highlighter, &show_matching_desc } });
    registry.insert({
        "show-whitespaces",
        { ShowWhitespacesHighlighter::create, &show_whitespace_desc } });
    registry.insert({
        "wrap",
        { WrapHighlighter::create, &wrap_desc } });
}

}
