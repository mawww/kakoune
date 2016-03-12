#include "highlighters.hh"

#include "assert.hh"
#include "buffer_utils.hh"
#include "changes.hh"
#include "context.hh"
#include "containers.hh"
#include "command_manager.hh"
#include "display_buffer.hh"
#include "face_registry.hh"
#include "highlighter_group.hh"
#include "line_modification.hh"
#include "option.hh"
#include "parameters_parser.hh"
#include "register_manager.hh"
#include "regex.hh"
#include "string.hh"
#include "window.hh"
#include "utf8.hh"
#include "utf8_iterator.hh"

#include <locale>
#include <cstdio>

namespace Kakoune
{

template<typename Func>
std::unique_ptr<Highlighter> make_highlighter(Func func, HighlightPass pass = HighlightPass::Colorize)
{
    struct SimpleHighlighter : public Highlighter
    {
        SimpleHighlighter(Func func, HighlightPass pass)
          : Highlighter{pass}, m_func{std::move(func)} {}

    private:
        void do_highlight(const Context& context, HighlightPass pass, DisplayBuffer& display_buffer, BufferRange range) override
        {
            m_func(context, pass, display_buffer, range);
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
        if (range.end <= begin or  end < range.begin)
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
    if (begin >= end or end <= display_buffer.range().begin
                     or begin >= display_buffer.range().end)
        return;

    for (auto& line : display_buffer.lines())
    {
        auto& range = line.range();
        if (range.end <= begin or  end < range.begin)
            continue;

        int beg_idx = -1, end_idx = -1;
        for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
        {
            if (not atom_it->has_buffer_range() or
                end <= atom_it->begin() or begin >= atom_it->end())
                continue;

            if (begin >= atom_it->begin())
            {
                if (begin > atom_it->begin())
                    atom_it = ++line.split(atom_it, begin);
                beg_idx = atom_it - line.begin();
            }
            if (end <= atom_it->end())
            {
                if (end < atom_it->end())
                    atom_it = line.split(atom_it, end);
                end_idx = (atom_it - line.begin()) + 1;
            }
        }

        if (beg_idx != -1 and end_idx != -1)
            func(line, beg_idx, end_idx);
    }
}

void apply_highlighter(const Context& context,
                       DisplayBuffer& display_buffer,
                       HighlightPass pass,
                       BufferCoord begin, BufferCoord end,
                       Highlighter& highlighter)
{
    if (begin == end)
        return;

    using LineIterator = DisplayBuffer::LineList::iterator;
    LineIterator first_line;
    Vector<DisplayLine::iterator> insert_pos;
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
        region_lines.emplace_back();
        insert_pos.emplace_back();

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

    if (region_display.lines().empty())
        return;

    region_display.compute_range();
    highlighter.highlight(context, pass, region_display, {begin, end});

    for (size_t i = 0; i < region_lines.size(); ++i)
    {
        auto& line = *(first_line + i);
        auto pos = insert_pos[i];
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

static HighlighterAndId create_fill_highlighter(HighlighterParameters params)
{
    if (params.size() != 1)
        throw runtime_error("wrong parameter count");

    const String& facespec = params[0];
    get_face(facespec); // validate param

    auto func = [=](const Context& context, HighlightPass pass,
                    DisplayBuffer& display_buffer, BufferRange range)
    {
        highlight_range(display_buffer, range.begin, range.end, true,
                        apply_face(get_face(facespec)));
    };
    return {"fill_" + facespec, make_highlighter(std::move(func))};
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

    void do_highlight(const Context& context, HighlightPass, DisplayBuffer& display_buffer, BufferRange range) override
    {
        auto overlaps = [](const BufferRange& lhs, const BufferRange& rhs) {
            return lhs.begin < rhs.begin ? lhs.end > rhs.begin
                                         : rhs.end > lhs.begin;
        };

        if (not overlaps(display_buffer.range(), range))
            return;

        Vector<Face> faces(m_faces.size());
        for (int f = 0; f < m_faces.size(); ++f)
        {
            if (not m_faces[f].second.empty())
                faces[f] = get_face(m_faces[f].second);
        }

        auto& matches = get_matches(context.buffer(), display_buffer.range(), range);
        kak_assert(matches.size() % m_faces.size() == 0);
        for (size_t m = 0; m < matches.size(); ++m)
        {
            auto& face = faces[m % faces.size()];
            if (face == Face{})
                continue;

            highlight_range(display_buffer,
                            matches[m].begin, matches[m].end,
                            true, apply_face(face));
        }
    }

    void reset(Regex regex, FacesSpec faces)
    {
        m_regex = std::move(regex);
        m_faces = std::move(faces);
        ensure_first_face_is_capture_0();
        ++m_regex_version;
    }

    static HighlighterAndId create(HighlighterParameters params)
    {
        if (params.size() < 2)
            throw runtime_error("wrong parameter count");

        FacesSpec faces;
        for (auto& spec : params.subrange(1))
        {
            auto colon = find(spec, ':');
            if (colon == spec.end())
                throw runtime_error(format("wrong face spec: '{}' expected <capture>:<facespec>", spec));
            get_face({colon+1, spec.end()}); // throw if wrong face spec
            int capture = str_to_int({spec.begin(), colon});
            faces.emplace_back(capture, String{colon+1, spec.end()});
        }

        String id = format("hlregex'{}'", params[0]);

        Regex ex{params[0], Regex::optimize};

        return {id, std::make_unique<RegexHighlighter>(std::move(ex),
                                                       std::move(faces))};
    }

private:
    // stores the range for each highlighted capture of each match
    using MatchList = Vector<BufferRange, MemoryDomain::Highlight>;
    struct Cache
    {
        size_t m_timestamp = -1;
        size_t m_regex_version = -1;
        struct RangeAndMatches { BufferRange range; MatchList matches; };
        Vector<RangeAndMatches, MemoryDomain::Highlight> m_matches;
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

    void add_matches(const Buffer& buffer, MatchList& matches,
                     BufferRange range)
    {
        kak_assert(matches.size() % m_faces.size() == 0);
        using RegexIt = RegexIterator<BufferIterator>;
        RegexIt re_it{get_iterator(buffer, range.begin),
                      get_iterator(buffer, range.end), m_regex,
                      match_flags(is_bol(range.begin),
                                  is_eol(buffer, range.end),
                                  is_bow(buffer, range.begin),
                                  is_eow(buffer, range.end))};
        RegexIt re_end;
        for (; re_it != re_end; ++re_it)
        {
            for (auto& face : m_faces)
            {
                const auto& sub = (*re_it)[face.first];
                matches.push_back({sub.first.coord(), sub.second.coord()});
            }
        }
    }

    MatchList& get_matches(const Buffer& buffer, BufferRange display_range,
                           BufferRange buffer_range)
    {
        Cache& cache = m_cache.get(buffer);
        auto& matches = cache.m_matches;

        if (cache.m_regex_version != m_regex_version or
            cache.m_timestamp != buffer.timestamp())
        {
            matches.clear();
            cache.m_timestamp = buffer.timestamp();
            cache.m_regex_version = m_regex_version;
        }
        const LineCount line_offset = 3;
        BufferRange range{std::max<BufferCoord>(buffer_range.begin, display_range.begin.line - line_offset),
                          std::min<BufferCoord>(buffer_range.end, display_range.end.line + line_offset)};

        auto it = std::upper_bound(matches.begin(), matches.end(), range,
                                   [](const BufferRange& lhs, const Cache::RangeAndMatches& rhs)
                                   { return lhs.begin < rhs.range.end; });

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
                old_range.begin = range.begin;
                MatchList new_matches;
                add_matches(buffer, new_matches, {range.begin, first_end});
                matches.erase(matches.begin(), matches.begin() + m_faces.size());

                std::copy(std::make_move_iterator(new_matches.begin()),
                          std::make_move_iterator(new_matches.end()),
                          std::inserter(matches, matches.begin()));
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

    void do_highlight(const Context& context, HighlightPass pass, DisplayBuffer& display_buffer, BufferRange range) override
    {
        Regex regex = m_regex_getter(context);
        FacesSpec face = m_face_getter(context);
        if (regex != m_last_regex or face != m_last_face)
        {
            m_last_regex = std::move(regex);
            m_last_face = face;
            if (not m_last_regex.empty())
                m_highlighter.reset(m_last_regex, m_last_face);
        }
        if (not m_last_regex.empty() and not m_last_face.empty())
            m_highlighter.highlight(context, pass, display_buffer, range);
    }

private:
    Regex       m_last_regex;
    RegexGetter m_regex_getter;

    FacesSpec   m_last_face;
    FaceGetter  m_face_getter;

    RegexHighlighter m_highlighter;
};

template<typename RegexGetter, typename FaceGetter>
std::unique_ptr<DynamicRegexHighlighter<RegexGetter, FaceGetter>>
make_dynamic_regex_highlighter(RegexGetter regex_getter, FaceGetter face_getter)
{
    return std::make_unique<DynamicRegexHighlighter<RegexGetter, FaceGetter>>(
        std::move(regex_getter), std::move(face_getter));
}

HighlighterAndId create_dynamic_regex_highlighter(HighlighterParameters params)
{
    if (params.size() < 2)
        throw runtime_error("Wrong parameter count");

    FacesSpec faces;
    for (auto& spec : params.subrange(1))
    {
        auto colon = find(spec, ':');
        if (colon == spec.end())
            throw runtime_error("wrong face spec: '" + spec +
                                 "' expected <capture>:<facespec>");
        get_face({colon+1, spec.end()}); // throw if wrong face spec
        int capture = str_to_int({spec.begin(), colon});
        faces.emplace_back(capture, String{colon+1, spec.end()});
    }

    auto get_face = [faces](const Context& context){ return faces;; };

    String expr = params[0];
    auto tokens = parse<true>(expr);
    if (tokens.size() == 1 and tokens[0].type() == Token::Type::OptionExpand and
        GlobalScope::instance().options()[tokens[0].content()].is_of_type<Regex>())
    {
        String option_name = tokens[0].content();
        auto get_regex =  [option_name](const Context& context) {
            return context.options()[option_name].get<Regex>();
        };
        return {format("dynregex_{}", expr), make_dynamic_regex_highlighter(get_regex, get_face)};
    }

    auto get_regex = [expr](const Context& context){
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
    return {format("dynregex_{}", expr), make_dynamic_regex_highlighter(get_regex, get_face)};
}

HighlighterAndId create_line_highlighter(HighlighterParameters params)
{
    if (params.size() != 2)
        throw runtime_error("wrong parameter count");

    String facespec = params[1];
    String line_expr = params[0];

    get_face(facespec); // validate facespec

    auto func = [=](const Context& context, HighlightPass, DisplayBuffer& display_buffer, BufferRange)
    {
        LineCount line = -1;
        try
        {
            line = str_to_int_ifp(expand(line_expr, context)).value_or(0) - 1;
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

        auto face = get_face(facespec);
        ColumnCount column = 0;
        for (auto& atom : *it)
        {
            column += atom.length();
            if (!atom.has_buffer_range())
                continue;

            kak_assert(atom.begin().line == line);
            apply_face(face)(atom);
        }
        const ColumnCount remaining = context.window().dimensions().column - column;
        if (remaining > 0)
            it->push_back({ String{' ', remaining}, face });
    };

    return {"hlline_" + params[0], make_highlighter(std::move(func))};
}

HighlighterAndId create_column_highlighter(HighlighterParameters params)
{
    if (params.size() != 2)
        throw runtime_error("wrong parameter count");

    String facespec = params[1];
    String col_expr = params[0];

    get_face(facespec); // validate facespec

    auto func = [=](const Context& context, HighlightPass, DisplayBuffer& display_buffer, BufferRange)
    {
        ColumnCount column = -1;
        try
        {
            column = str_to_int_ifp(expand(col_expr, context)).value_or(0) - 1;
        }
        catch (runtime_error& err)
        {
            write_to_debug_buffer(
                format("Error evaluating highlight column expression: {}", err.what()));
        }

        if (column < 0)
            return;

        const Buffer& buffer = context.buffer();
        const int tabstop = context.options()["tabstop"].get<int>();
        auto face = get_face(facespec);
        for (auto& line : display_buffer.lines())
        {
            const LineCount buf_line = line.range().begin.line;
            const ByteCount byte_col = get_byte_to_column(buffer, tabstop, {buf_line, column});
            const BufferCoord coord{buf_line, byte_col};
            bool found = false;
            if (buffer.is_valid(coord) and not buffer.is_end(coord))
            {
                for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
                {
                    if (atom_it->type() != DisplayAtom::Range)
                        continue;

                    kak_assert(atom_it->begin().line == buf_line);
                    if (coord >= atom_it->begin() and coord < atom_it->end())
                    {
                        if (coord > atom_it->begin())
                            atom_it = ++line.split(atom_it, coord);
                        if (buffer.next(coord) < atom_it->end())
                            atom_it = line.split(atom_it, buffer.next(coord));

                        apply_face(face)(*atom_it);
                        found = true;
                        break;
                    }
                }
            }
            if (not found)
            {
                ColumnCount last_buffer_col = context.window().position().column;
                for (auto& atom : line)
                {
                    if (atom.has_buffer_range())
                    {
                        auto pos = atom.end();
                        if (pos.column == 0)
                            pos = {pos.line-1, buffer[pos.line-1].length()};
                        if (pos != atom.begin())
                            last_buffer_col = get_column(buffer, tabstop, pos);
                    }
                }

                ColumnCount count = column - last_buffer_col;
                if (count >= 0)
                {
                    if (count > 0)
                        line.push_back({String{' ',  count}});
                    line.push_back({String{" "}, face});
                }
            }
        }
    };

    return {"hlcol_" + params[0], make_highlighter(std::move(func))};
}

struct WrapHighlighter : Highlighter
{
    WrapHighlighter(ColumnCount max_width, bool word_wrap)
        : Highlighter{HighlightPass::Wrap}, m_max_width{max_width}, m_word_wrap{word_wrap} {}

    void do_highlight(const Context& context, HighlightPass pass,
                      DisplayBuffer& display_buffer, BufferRange) override
    {
        const ColumnCount wrap_column = std::min(m_max_width, context.window().range().column);
        if (wrap_column <= 0)
            return;

        const Buffer& buffer = context.buffer();
        const auto& cursor = context.selections().main().cursor();
        const int tabstop = context.options()["tabstop"].get<int>();
        const LineCount win_height = context.window().dimensions().line;
        for (auto it = display_buffer.lines().begin();
             it != display_buffer.lines().end(); ++it)
        {
            const LineCount buf_line = it->range().begin.line;
            const ByteCount line_length = buffer[buf_line].length();

            auto coord = next_split_coord(buffer, wrap_column, tabstop, buf_line);
            if (buffer.is_valid(coord) and not buffer.is_end(coord))
            {
                for (auto atom_it = it->begin();
                     coord.column != line_length and atom_it != it->end(); )
                {
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

                    if (it+1 - display_buffer.lines().begin() == win_height)
                    {
                        if (cursor >= new_line.begin()->begin()) // strip first lines if cursor is not visible
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

                    coord = next_split_coord(buffer, wrap_column, tabstop, coord);
                    atom_it = it->begin();
                }
            }
        }
    }

    void do_compute_display_setup(const Context& context, HighlightPass, DisplaySetup& setup) override
    {
        const ColumnCount wrap_column = std::min(setup.window_range.column, m_max_width);
        if (wrap_column <= 0)
            return;

        const Buffer& buffer = context.buffer();
        const auto& cursor = context.selections().main().cursor();
        const int tabstop = context.options()["tabstop"].get<int>();

        auto line_wrap_count = [&](LineCount line) {
            LineCount count = 0;
            BufferCoord coord{line};
            const ByteCount line_length = buffer[line].length();
            while (true)
            {
                coord = next_split_coord(buffer, wrap_column, tabstop, coord);
                if (coord.column == line_length)
                    break;
                ++count;
            }
            return count;
        };

        // Disable horizontal scrolling when using a WrapHighlighter
        setup.cursor_pos.column += setup.window_pos.column;
        setup.window_pos.column = 0;
        setup.scroll_offset.column = 0;
        setup.full_lines = true;

        const LineCount win_height = context.window().dimensions().line;
        LineCount win_line = 0;
        for (auto buf_line = setup.window_pos.line;
             buf_line < setup.window_pos.line + setup.window_range.line;
             ++buf_line)
        {
            if (buf_line >= buffer.line_count())
                break;

            const auto wrap_count = line_wrap_count(buf_line);
            setup.window_range.line -= wrap_count;

            if (win_line < setup.cursor_pos.line)
                setup.cursor_pos.line += wrap_count;
            // Place the cursor correctly after its line gets wrapped
            else if (win_line == setup.cursor_pos.line)
            {
                BufferCoord coord{buf_line};
                while (true)
                {
                    auto split_coord = next_split_coord(buffer, wrap_column, tabstop, coord);
                    if (split_coord.column > cursor.column)
                    {
                        setup.cursor_pos.column = get_column(buffer, tabstop, cursor) - get_column(buffer, tabstop, coord);
                        break;
                    }
                    ++setup.cursor_pos.line;
                    coord = split_coord;
                }
                kak_assert(setup.cursor_pos.column >= 0 and setup.cursor_pos.column < setup.window_range.column);
                if (setup.cursor_pos.line >= win_height) // In that case we will remove some lines from the top
                    setup.cursor_pos.line = win_height - 1;
            }
            win_line += wrap_count + 1;

            // scroll window to keep cursor visible, and update range as lines gets removed
            while (setup.window_pos.line < cursor.line and
                   setup.cursor_pos.line >= win_height - setup.scroll_offset.line)
            {
                auto removed_lines = 1 + line_wrap_count(setup.window_pos.line++);
                setup.cursor_pos.line -= removed_lines;
                win_line -= removed_lines;
                // removed one line from the range, added removed_lines potential ones
                setup.window_range.line += removed_lines - 1;
                kak_assert(setup.cursor_pos.line >= 0);
            }

            if (setup.window_range.line <= buf_line - setup.window_pos.line)
                setup.window_range.line = buf_line - setup.window_pos.line + 1;
            // Todo: support displaying partial lines, so that we can ensure the cursor is
            // visible even if a line takes more than the full screen height once wrapped.
            // kak_assert(setup.cursor_pos.line >= 0 and setup.cursor_pos.line < win_height);
        }
    }

    BufferCoord next_split_coord(const Buffer& buffer,  ColumnCount wrap_column, int tabstop, BufferCoord coord)
    {
        auto column = get_column(buffer, tabstop, coord);
        auto col = get_byte_to_column(
            buffer, tabstop, {coord.line, column + wrap_column});
        BufferCoord split_coord{coord.line, col};

        if (m_word_wrap)
        {
            StringView line = buffer[coord.line];
            utf8::iterator<const char*> it{&line[col], line};
            while (it != line.end() and it != line.begin() and is_word(*it))
                --it;

            if (it != line.begin() and it != &line[col] and
                (it+1) > &line[coord.column])
                split_coord.column = (it+1).base() - line.begin();
        }
        return split_coord;
    };

    static HighlighterAndId create(HighlighterParameters params)
    {
        static const ParameterDesc param_desc{
            { { "word", { false, "" } },
              { "width", { true, "" } } },
            ParameterDesc::Flags::None, 0, 0
        };
        ParametersParser parser(params, param_desc);

        ColumnCount max_width{std::numeric_limits<int>::max()};
        if (auto width = parser.get_switch("width"))
            max_width = str_to_int(*width);

        return {"wrap", std::make_unique<WrapHighlighter>(max_width, (bool)parser.get_switch("word"))};
    }

    const bool m_word_wrap;
    const ColumnCount m_max_width;
};

struct TabulationHighlighter : Highlighter
{
    TabulationHighlighter() : Highlighter{HighlightPass::Move} {}

    void do_highlight(const Context& context, HighlightPass,
                      DisplayBuffer& display_buffer, BufferRange) override
    {
        const ColumnCount tabstop = context.options()["tabstop"].get<int>();
        auto& buffer = context.buffer();
        auto win_column = context.window().position().column;
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
                        const ColumnCount count = tabstop - (column % tabstop) -
                                                  std::max(win_column - column, 0_col);
                        atom_it->replace(String{' ', count});
                        break;
                    }
                }
            }
        }
    }

    void do_compute_display_setup(const Context& context, HighlightPass, DisplaySetup& setup) override
    {
        auto& buffer = context.buffer();
        // Ensure that a cursor on a tab character makes the full tab character visible
        auto cursor = context.selections().main().cursor();
        if (buffer.byte_at(cursor) != '\t')
            return;

        const ColumnCount tabstop = context.options()["tabstop"].get<int>();
        const ColumnCount column = get_column(buffer, tabstop, cursor);
        const ColumnCount width = tabstop - (column % tabstop);
        const ColumnCount win_end = setup.window_pos.column + setup.window_range.column;
        const ColumnCount offset = std::max(column + width - win_end, 0_col);

        setup.window_pos.column += offset;
        setup.cursor_pos.column -= offset;
    }
};

void show_whitespaces(const Context& context, HighlightPass, DisplayBuffer& display_buffer, BufferRange,
                      StringView tab, StringView tabpad,
                      StringView spc, StringView lf, StringView nbsp)
{
    const int tabstop = context.options()["tabstop"].get<int>();
    auto whitespaceface = get_face("Whitespace");
    auto& buffer = context.buffer();
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
                Codepoint cp = utf8::read_codepoint<utf8::InvalidPolicy::Pass>(it, end);
                if (cp == '\t' or cp == ' ' or cp == '\n' or cp == 0xA0)
                {
                    if (coord != begin.coord())
                        atom_it = ++line.split(atom_it, coord);
                    if (it != end)
                        atom_it = line.split(atom_it, it.coord());

                    if (cp == '\t')
                    {
                        int column = (int)get_column(buffer, tabstop, coord);
                        int count = tabstop - (column % tabstop);
                        atom_it->replace(tab + String(tabpad[(CharCount)0], CharCount{count-1}));
                    }
                    else if (cp == ' ')
                        atom_it->replace(spc.str());
                    else if (cp == '\n')
                        atom_it->replace(lf.str());
                    else if (cp == 0xA0)
                        atom_it->replace(nbsp.str());
                    atom_it->face = merge_faces(atom_it->face, whitespaceface);
                    break;
                }
            }
        }
    }
}

HighlighterAndId show_whitespaces_factory(HighlighterParameters params)
{
    static const ParameterDesc param_desc{
        { { "tab", { true, "" } },
          { "tabpad", { true, "" } },
          { "spc", { true, "" } },
          { "lf", { true, "" } },
          { "nbsp", { true, "" } } },
        ParameterDesc::Flags::None, 0, 0
    };
    ParametersParser parser(params, param_desc);

    auto get_param = [&](StringView param,  StringView fallback) {
        StringView value = parser.get_switch(param).value_or(fallback);
        if (value.char_length() != 1)
            throw runtime_error{format("-{} expects a single character parmeter", param)};
        return value.str();
    };

    using namespace std::placeholders;
    auto func = std::bind(show_whitespaces, _1, _2, _3, _4,
                          get_param("tab", "→"), get_param("tabpad", " "),
                          get_param("spc", "·"),
                          get_param("lf", "¬"),
                          get_param("nbsp", "⍽"));

    return {"show_whitespaces", make_highlighter(std::move(func))};
}

struct LineNumbersHighlighter : Highlighter
{
    LineNumbersHighlighter(bool relative, bool hl_cursor_line, String separator)
      : Highlighter{HighlightPass::Move},
        m_relative{relative},
        m_hl_cursor_line{hl_cursor_line},
        m_separator{std::move(separator)} {}

    static HighlighterAndId create(HighlighterParameters params)
    {
        static const ParameterDesc param_desc{
            { { "relative", { false, "" } },
              { "separator", { true, "" } },
              { "hlcursor", { false, "" } } },
            ParameterDesc::Flags::None, 0, 0
        };
        ParametersParser parser(params, param_desc);

        StringView separator = parser.get_switch("separator").value_or("│");
        if (separator.length() > 10)
            throw runtime_error("Separator length is limited to 10 bytes");

        return {"number_lines", std::make_unique<LineNumbersHighlighter>((bool)parser.get_switch("relative"), (bool)parser.get_switch("hlcursor"), separator.str())};
    }

private:
    void do_highlight(const Context& context, HighlightPass, DisplayBuffer& display_buffer, BufferRange) override
    {
        const Face face = get_face("LineNumbers");
        const Face face_wrapped = get_face("LineNumbersWrapped");
        const Face face_absolute = get_face("LineNumberCursor");
        int digit_count = compute_digit_count(context);

        char format[16];
        format_to(format, "%{}d", digit_count);
        const int main_line = (int)context.selections().main().cursor().line + 1;
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
            line.insert(line.begin(), {buffer, atom_face});
            line.insert(line.begin() + 1, {m_separator, face});

            last_line = current_line;
        }
    }

    void do_compute_display_setup(const Context& context, HighlightPass, DisplaySetup& setup) override
    {
        ColumnCount width = compute_digit_count(context) + m_separator.column_length();
        setup.window_range.column -= width;
    }

    int compute_digit_count(const Context& context)
    {
        int digit_count = 0;
        LineCount last_line = context.buffer().line_count();
        for (LineCount c = last_line; c > 0; c /= 10)
            ++digit_count;
        return digit_count;
    }

   const bool m_relative;
   const bool m_hl_cursor_line;
   const String m_separator;
};


void show_matching_char(const Context& context, HighlightPass, DisplayBuffer& display_buffer, BufferRange)
{
    const Face face = get_face("MatchingChar");
    using CodepointPair = std::pair<Codepoint, Codepoint>;
    static const CodepointPair matching_chars[] = { { '(', ')' }, { '{', '}' }, { '[', ']' }, { '<', '>' } };
    const auto range = display_buffer.range();
    const auto& buffer = context.buffer();
    for (auto& sel : context.selections())
    {
        auto pos = sel.cursor();
        if (pos < range.begin or pos >= range.end)
            continue;
        auto c = buffer.byte_at(pos);
        for (auto& pair : matching_chars)
        {
            int level = 1;
            if (c == pair.first)
            {
                for (auto it = get_iterator(buffer, pos)+1,
                         end = get_iterator(buffer, range.end); it != end; ++it)
                {
                    char c = *it;
                    if (c == pair.first)
                        ++level;
                    else if (c == pair.second and --level == 0)
                    {
                        highlight_range(display_buffer, it.coord(), (it+1).coord(), false,
                                        apply_face(face));
                        break;
                    }
                }
            }
            else if (c == pair.second and pos > range.begin)
            {
                for (auto it = get_iterator(buffer, pos)-1,
                         end = get_iterator(buffer, range.begin); true; --it)
                {
                    char c = *it;
                    if (c == pair.second)
                        ++level;
                    else if (c == pair.first and --level == 0)
                    {
                        highlight_range(display_buffer, it.coord(), (it+1).coord(), false,
                                        apply_face(face));
                        break;
                    }
                    if (it == end)
                        break;
                }
            }
        }
    }
}

HighlighterAndId create_matching_char_highlighter(HighlighterParameters params)
{
    return {"show_matching", make_highlighter(show_matching_char)};
}

void highlight_selections(const Context& context, HighlightPass, DisplayBuffer& display_buffer, BufferRange)
{
    const auto& buffer = context.buffer();
    const Face primary_face = get_face("PrimarySelection");
    const Face secondary_face = get_face("SecondarySelection");
    const Face primary_cursor_face = get_face("PrimaryCursor");
    const Face secondary_cursor_face = get_face("SecondaryCursor");

    const auto& selections = context.selections();
    for (size_t i = 0; i < selections.size(); ++i)
    {
        auto& sel = selections[i];
        const bool forward = sel.anchor() <= sel.cursor();
        BufferCoord begin = forward ? sel.anchor() : buffer.char_next(sel.cursor());
        BufferCoord end   = forward ? (BufferCoord)sel.cursor() : buffer.char_next(sel.anchor());

        const bool primary = (i == selections.main_index());
        highlight_range(display_buffer, begin, end, false,
                        apply_face(primary ? primary_face : secondary_face));
    }
    for (size_t i = 0; i < selections.size(); ++i)
    {
        auto& sel = selections[i];
        const bool primary = (i == selections.main_index());
        highlight_range(display_buffer, sel.cursor(), buffer.char_next(sel.cursor()), false,
                        apply_face(primary ? primary_cursor_face : secondary_cursor_face));
    }
}

void expand_unprintable(const Context& context, HighlightPass, DisplayBuffer& display_buffer, BufferRange)
{
    auto& buffer = context.buffer();
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
                    Codepoint cp = utf8::read_codepoint<utf8::InvalidPolicy::Pass>(it, end);
                    if (cp != '\n' and not iswprint((wchar_t)cp))
                    {
                        if (coord != atom_it->begin())
                            atom_it = ++line.split(atom_it, coord);
                        if (it.coord() < atom_it->end())
                            atom_it = line.split(atom_it, it.coord());

                        atom_it->replace(format("U+{}", hex(cp)));
                        atom_it->face = { Color::Red, Color::Black };
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

    std::sort(lines.begin(), lines.end(),
              [](const LineAndSpec& lhs, const LineAndSpec& rhs)
              { return std::get<0>(lhs) < std::get<0>(rhs); });

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

struct FlagLinesHighlighter : Highlighter
{
    FlagLinesHighlighter(String option_name, String default_face)
        : Highlighter{HighlightPass::Move},
          m_option_name{std::move(option_name)},
          m_default_face{std::move(default_face)} {}

    static HighlighterAndId create(HighlighterParameters params)
    {
        if (params.size() != 2)
            throw runtime_error("wrong parameter count");

        const String& option_name = params[1];
        const String& default_face = params[0];
        get_face(default_face); // validate param

        // throw if wrong option type
        GlobalScope::instance().options()[option_name].get<LineAndSpecList>();

        return {"hlflags_" + params[1], std::make_unique<FlagLinesHighlighter>(option_name, default_face) };
    }

private:
    void do_highlight(const Context& context, HighlightPass,
                      DisplayBuffer& display_buffer, BufferRange) override
    {
        auto& line_flags = context.options()[m_option_name].get_mutable<LineAndSpecList>();
        auto& buffer = context.buffer();
        update_line_specs_ifn(buffer, line_flags);

        auto def_face = get_face(m_default_face);
        Vector<DisplayLine> display_lines;
        auto& lines = line_flags.list;
        try
        {
            for (auto& line : lines)
            {
                display_lines.push_back(parse_display_line(std::get<1>(line)));
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

    void do_compute_display_setup(const Context& context, HighlightPass, DisplaySetup& setup) override
    {
        auto& line_flags = context.options()[m_option_name].get_mutable<LineAndSpecList>();
        auto& buffer = context.buffer();
        update_line_specs_ifn(buffer, line_flags);

        ColumnCount width = 0;
        try 
        {
            for (auto& line : line_flags.list)
                width = std::max(parse_display_line(std::get<1>(line)).length(), width);
        }
        catch (runtime_error& err)
        {
            write_to_debug_buffer(format("Error while evaluating line flag: {}", err.what()));
            return;
        }

        setup.window_range.column -= width;
    }

    String m_option_name;
    String m_default_face;
};

String option_to_string(InclusiveBufferRange range)
{
    return format("{}.{},{}.{}",
                  range.first.line+1, range.first.column+1,
                  range.last.line+1, range.last.column+1);
}

void option_from_string(StringView str, InclusiveBufferRange& opt)
{
    auto sep = find_if(str, [](char c){ return c == ',' or c == '+'; });
    auto dot_beg = find(StringView{str.begin(), sep}, '.');
    auto dot_end = find(StringView{sep, str.end()}, '.');

    if (sep == str.end() or dot_beg == sep or
        (*sep == ',' and dot_end == str.end()))
        throw runtime_error(format("'{}' does not follow <line>.<column>,<line>.<column> or <line>.<column>+<len> format", str));

    const BufferCoord first{str_to_int({str.begin(), dot_beg}) - 1,
                            str_to_int({dot_beg+1, sep}) - 1};

    const bool len = (*sep == '+');
    const BufferCoord last{len ? first.line : str_to_int({sep+1, dot_end}) - 1,
                           len ? first.column + str_to_int({sep+1, str.end()}) - 1
                               : str_to_int({dot_end+1, str.end()}) - 1 };

    if (first.line < 0 or first.column < 0 or last.line < 0 or last.column < 0)
        throw runtime_error("coordinates elements should be >= 1");

    opt = { std::min(first, last), std::max(first, last) };
}

BufferCoord& get_first(RangeAndString& r) { return std::get<0>(r).first; }
BufferCoord& get_last(RangeAndString& r) { return std::get<0>(r).last; }

static void update_ranges_ifn(const Buffer& buffer, RangeAndStringList& range_and_faces)
{
    if (range_and_faces.prefix == buffer.timestamp())
        return;

    auto changes = buffer.changes_since(range_and_faces.prefix);
    for (auto change_it = changes.begin(); change_it != changes.end(); )
    {
        auto forward_end = forward_sorted_until(change_it, changes.end());
        auto backward_end = backward_sorted_until(change_it, changes.end());

        if (forward_end >= backward_end)
        {
            update_forward({ change_it, forward_end }, range_and_faces.list);
            change_it = forward_end;
        }
        else
        {
            update_backward({ change_it, backward_end }, range_and_faces.list);
            change_it = backward_end;
        }
    }
    range_and_faces.prefix = buffer.timestamp();
}

void option_update(RangeAndStringList& opt, const Context& context)
{
    update_ranges_ifn(context.buffer(), opt);
}

struct RangesHighlighter : Highlighter
{
    RangesHighlighter(String option_name)
        : Highlighter{HighlightPass::Colorize}
        , m_option_name{std::move(option_name)} {}

    static HighlighterAndId create(HighlighterParameters params)
    {
        if (params.size() != 1)
            throw runtime_error("wrong parameter count");

        const String& option_name = params[0];
        // throw if wrong option type
        GlobalScope::instance().options()[option_name].get<RangeAndStringList>();

        return {"hlranges_" + params[0], std::make_unique<RangesHighlighter>(option_name)};
    }

private:
    void do_highlight(const Context& context, HighlightPass, DisplayBuffer& display_buffer, BufferRange) override
    {
        auto& buffer = context.buffer();
        auto& range_and_faces = context.options()[m_option_name].get_mutable<RangeAndStringList>();
        update_ranges_ifn(buffer, range_and_faces);

        for (auto& range : range_and_faces.list)
        {
            try
            {
                auto& r = std::get<0>(range);
                if (buffer.is_valid(r.first) and buffer.is_valid(r.last))
                    highlight_range(display_buffer, r.first, buffer.char_next(r.last), true,
                                    apply_face(get_face(std::get<1>(range))));
            }
            catch (runtime_error&)
            {}
        }
    }

    const String m_option_name;
};

struct ReplaceRangesHighlighter : Highlighter
{
    ReplaceRangesHighlighter(String option_name)
        : Highlighter{HighlightPass::Colorize}
        , m_option_name{std::move(option_name)} {}

    static HighlighterAndId create(HighlighterParameters params)
    {
        if (params.size() != 1)
            throw runtime_error("wrong parameter count");

        const String& option_name = params[0];
        // throw if wrong option type
        GlobalScope::instance().options()[option_name].get<RangeAndStringList>();

        return {"replace_ranges_" + params[0], std::make_unique<ReplaceRangesHighlighter>(option_name)};
    }

private:
    void do_highlight(const Context& context, HighlightPass, DisplayBuffer& display_buffer, BufferRange) override
    {
        auto& buffer = context.buffer();
        auto& range_and_faces = context.options()[m_option_name].get_mutable<RangeAndStringList>();
        update_ranges_ifn(buffer, range_and_faces);

        for (auto& range : range_and_faces.list)
        {
            try
            {
                auto& r = std::get<0>(range);
                if (buffer.is_valid(r.first) and buffer.is_valid(r.last))
                {
                    auto replacement = parse_display_line(std::get<1>(range));
                    replace_range(display_buffer, r.first, buffer.char_next(r.last),
                                  [&](DisplayLine& line, int beg_idx, int end_idx){
                                      auto it = line.erase(line.begin() + beg_idx, line.begin() + end_idx);
                                      for (auto& atom : replacement)
                                          it = ++line.insert(it, std::move(atom));
                                  });
                }
            }
            catch (runtime_error&)
            {}
        }
    }

    const String m_option_name;
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

HighlighterAndId create_highlighter_group(HighlighterParameters params)
{
    static const ParameterDesc param_desc{
        { { "passes", { true, "" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart, 1, 1
    };
    ParametersParser parser{params, param_desc};
    HighlightPass passes = parse_passes(parser.get_switch("passes").value_or("colorize"));

    return HighlighterAndId(parser[0], std::make_unique<HighlighterGroup>(passes));
}

struct ReferenceHighlighter : Highlighter
{
    ReferenceHighlighter(HighlightPass passes, String name)
        : Highlighter{passes}, m_name{std::move(name)} {}

    static HighlighterAndId create(HighlighterParameters params)
    {
        static const ParameterDesc param_desc{
            { { "passes", { true, "" } } },
            ParameterDesc::Flags::SwitchesOnlyAtStart, 1, 1
        };
        ParametersParser parser{params, param_desc};
        HighlightPass passes = parse_passes(parser.get_switch("passes").value_or("colorize"));
        return {parser[0], std::make_unique<ReferenceHighlighter>(passes, parser[0])};
    }

private:
    void do_highlight(const Context& context, HighlightPass pass,
                      DisplayBuffer& display_buffer, BufferRange range) override
    {
        try
        {
            DefinedHighlighters::instance().get_child(m_name).highlight(context, pass, display_buffer, range);
        }
        catch (child_not_found&)
        {}
    }

    void do_compute_display_setup(const Context& context, HighlightPass pass, DisplaySetup& setup) override
    {
        try
        {
            DefinedHighlighters::instance().get_child(m_name).compute_display_setup(context, pass, setup);
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
    StringView capture;

    BufferCoord begin_coord() const { return { line, begin }; }
    BufferCoord end_coord() const { return { line, end }; }
};
using RegexMatchList = Vector<RegexMatch, MemoryDomain::Highlight>;

void find_matches(const Buffer& buffer, RegexMatchList& matches, const Regex& regex, bool capture)
{
    capture = capture and regex.mark_count() > 0;
    for (auto line = 0_line, end = buffer.line_count(); line < end; ++line)
    {
        auto l = buffer[line];
        for (RegexIterator<const char*> it{l.begin(), l.end(), regex}, end{}; it != end; ++it)
        {
            auto& m = *it;
            ByteCount b = (int)(m[0].first - l.begin());
            ByteCount e = (int)(m[0].second - l.begin());
            auto cap = (capture and m[1].matched) ? StringView{m[1].first, m[1].second} : StringView{};
            matches.push_back({ line, b, e, cap });
        }
    }
}

void update_matches(const Buffer& buffer, ConstArrayView<LineModification> modifs,
                    RegexMatchList& matches, const Regex& regex, bool capture)
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
    size_t pivot = matches.size();

    // try to find new matches in each updated lines
    capture = capture and regex.mark_count() > 0;
    for (auto& modif : modifs)
    {
        for (auto line = modif.new_line; line < modif.new_line + modif.num_added; ++line)
        {
            auto l = buffer[line];
            for (RegexIterator<const char*> it{l.begin(), l.end(), regex}, end{}; it != end; ++it)
            {
                auto& m = *it;
                ByteCount b = (int)(m[0].first - l.begin());
                ByteCount e = (int)(m[0].second - l.begin());
                auto cap = (capture and m[1].matched) ? StringView{m[1].first, m[1].second} : StringView{};
                matches.push_back({ line, b, e, cap });
            }
        }
    }
    std::inplace_merge(matches.begin(), matches.begin() + pivot, matches.end(),
                       [](const RegexMatch& lhs, const RegexMatch& rhs) {
                           return lhs.begin_coord() < rhs.begin_coord();
                       });
}

struct RegionMatches
{
    RegexMatchList begin_matches;
    RegexMatchList end_matches;
    RegexMatchList recurse_matches;

    static bool compare_to_begin(const RegexMatch& lhs, BufferCoord rhs)
    {
        return lhs.begin_coord() < rhs;
    }

    RegexMatchList::const_iterator find_next_begin(BufferCoord pos) const
    {
        return std::lower_bound(begin_matches.begin(), begin_matches.end(),
                                pos, compare_to_begin);
    }

    RegexMatchList::const_iterator find_matching_end(BufferCoord beg_pos, Optional<StringView> capture) const
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
                   rec_it->end_coord() <= end_it->begin_coord())
            {
                if (not capture or rec_it->capture == *capture)
                    ++recurse_level;
                ++rec_it;
            }

            if (not capture or *capture == end_it->capture)
            {
                if (recurse_level == 0)
                    return end_it;
                --recurse_level;
            }

            beg_pos = end_it->end_coord();
        }
    }
};

struct RegionDesc
{
    String m_name;
    Regex m_begin;
    Regex m_end;
    Regex m_recurse;
    bool  m_match_capture;

    RegionMatches find_matches(const Buffer& buffer) const
    {
        RegionMatches res;
        Kakoune::find_matches(buffer, res.begin_matches, m_begin, m_match_capture);
        Kakoune::find_matches(buffer, res.end_matches, m_end, m_match_capture);
        if (not m_recurse.empty())
            Kakoune::find_matches(buffer, res.recurse_matches, m_recurse, m_match_capture);
        return res;
    }

    void update_matches(const Buffer& buffer,
                        ConstArrayView<LineModification> modifs,
                        RegionMatches& matches) const
    {
        Kakoune::update_matches(buffer, modifs, matches.begin_matches, m_begin, m_match_capture);
        Kakoune::update_matches(buffer, modifs, matches.end_matches, m_end, m_match_capture);
        if (not m_recurse.empty())
            Kakoune::update_matches(buffer, modifs, matches.recurse_matches, m_recurse, m_match_capture);
    }
};

struct RegionsHighlighter : public Highlighter
{
public:
    using RegionDescList = Vector<RegionDesc, MemoryDomain::Highlight>;

    RegionsHighlighter(RegionDescList regions, String default_group)
        : Highlighter{HighlightPass::Colorize},
          m_regions{std::move(regions)},
          m_default_group{std::move(default_group)}
    {
        if (m_regions.empty())
            throw runtime_error("at least one region must be defined");

        for (auto& region : m_regions)
        {
            m_groups.insert({region.m_name, HighlighterGroup{HighlightPass::Colorize}});
            if (region.m_begin.empty() or region.m_end.empty())
                throw runtime_error("invalid regex for region highlighter");
        }
        if (not m_default_group.empty())
            m_groups.insert({m_default_group, HighlighterGroup{HighlightPass::Colorize}});
    }

    void do_highlight(const Context& context, HighlightPass pass, DisplayBuffer& display_buffer, BufferRange range) override
    {
        auto display_range = display_buffer.range();
        const auto& buffer = context.buffer();
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

        auto default_group_it = m_groups.find(m_default_group);
        const bool apply_default = default_group_it != m_groups.end();

        auto last_begin = (begin == regions.begin()) ?
                             BufferCoord{0,0} : (begin-1)->end;
        kak_assert(begin <= end);
        for (; begin != end; ++begin)
        {
            if (apply_default and last_begin < begin->begin)
                apply_highlighter(context, display_buffer, pass,
                                  correct(last_begin), correct(begin->begin),
                                  default_group_it->value);

            auto it = m_groups.find(begin->group);
            if (it == m_groups.end())
                continue;
            apply_highlighter(context, display_buffer, pass,
                              correct(begin->begin), correct(begin->end),
                              it->value);
            last_begin = begin->end;
        }
        if (apply_default and last_begin < display_range.end)
            apply_highlighter(context, display_buffer, pass,
                              correct(last_begin), range.end,
                              default_group_it->value);
    }

    bool has_children() const override { return true; }

    Highlighter& get_child(StringView path) override
    {
        auto sep_it = find(path, '/');
        StringView id(path.begin(), sep_it);
        auto it = m_groups.find(id);
        if (it == m_groups.end())
            throw child_not_found(format("no such id: {}", id));
        if (sep_it == path.end())
            return it->value;
        else
            return it->value.get_child({sep_it+1, path.end()});
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

        auto container = m_groups | transform(std::mem_fn(&decltype(m_groups)::Item::key));
        return { 0, 0, complete(path, cursor_pos, container) };
    }

    static HighlighterAndId create(HighlighterParameters params)
    {
        static const ParameterDesc param_desc{
            { { "default", { true, "" } }, { "match-capture", { false, "" } } },
            ParameterDesc::Flags::SwitchesOnlyAtStart, 5
        };

        ParametersParser parser{params, param_desc};
        if ((parser.positional_count() % 4) != 1)
            throw runtime_error("wrong parameter count, expected <id> (<group name> <begin> <end> <recurse>)+");

        const bool match_capture = (bool)parser.get_switch("match-capture");
        RegionsHighlighter::RegionDescList regions;
        for (size_t i = 1; i < parser.positional_count(); i += 4)
        {
            if (parser[i].empty() or parser[i+1].empty() or parser[i+2].empty())
                throw runtime_error("group id, begin and end must not be empty");

            const Regex::flag_type flags = match_capture ?
                Regex::optimize : Regex::nosubs | Regex::optimize;

            regions.push_back({ parser[i],
                                Regex{parser[i+1], flags}, Regex{parser[i+2], flags},
                                parser[i+3].empty() ? Regex{} : Regex{parser[i+3], flags},
                                match_capture });
        }

        auto default_group = parser.get_switch("default").value_or(StringView{}).str();
        return {parser[0], std::make_unique<RegionsHighlighter>(std::move(regions), default_group)};
    }

private:
    const RegionDescList m_regions;
    const String m_default_group;
    HashMap<String, HighlighterGroup, MemoryDomain::Highlight> m_groups;

    struct Region
    {
        BufferCoord begin;
        BufferCoord end;
        StringView group;
    };
    using RegionList = Vector<Region, MemoryDomain::Highlight>;

    struct Cache
    {
        size_t timestamp = 0;
        Vector<RegionMatches, MemoryDomain::Highlight> matches;
        HashMap<BufferRange, RegionList, MemoryDomain::Highlight> regions;
    };
    BufferSideCache<Cache> m_cache;

    using RegionAndMatch = std::pair<size_t, RegexMatchList::const_iterator>;

    // find the begin closest to pos in all matches
    RegionAndMatch find_next_begin(const Cache& cache, BufferCoord pos) const
    {
        RegionAndMatch res{0, cache.matches[0].find_next_begin(pos)};
        for (size_t i = 1; i < cache.matches.size(); ++i)
        {
            const auto& matches = cache.matches[i];
            auto it = matches.find_next_begin(pos);
            if (it != matches.begin_matches.end() and
                (res.second == cache.matches[res.first].begin_matches.end() or
                 it->begin_coord() < res.second->begin_coord()))
                res = RegionAndMatch{i, it};
        }
        return res;
    }

    const RegionList& get_regions_for_range(const Buffer& buffer, BufferRange range)
    {
        Cache& cache = m_cache.get(buffer);
        const size_t buf_timestamp = buffer.timestamp();
        if (cache.timestamp != buf_timestamp)
        {
            if (cache.timestamp == 0)
            {
                cache.matches.resize(m_regions.size());
                for (size_t i = 0; i < m_regions.size(); ++i)
                    cache.matches[i] = m_regions[i].find_matches(buffer);
            }
            else
            {
                auto modifs = compute_line_modifications(buffer, cache.timestamp);
                for (size_t i = 0; i < m_regions.size(); ++i)
                    m_regions[i].update_matches(buffer, modifs, cache.matches[i]);
            }

            cache.regions.clear();
        }

        auto it = cache.regions.find(range);
        if (it != cache.regions.end())
            return it->value;

        RegionList& regions = cache.regions[range];

        for (auto begin = find_next_begin(cache, range.begin),
                  end = RegionAndMatch{ 0, cache.matches[0].begin_matches.end() };
             begin != end; )
        {
            const RegionMatches& matches = cache.matches[begin.first];
            auto& region = m_regions[begin.first];
            auto beg_it = begin.second;
            auto end_it = matches.find_matching_end(beg_it->end_coord(),
                                                    region.m_match_capture ? beg_it->capture
                                                                           : Optional<StringView>{});

            if (end_it == matches.end_matches.end() or end_it->end_coord() >= range.end)
            {
                regions.push_back({ {beg_it->line, beg_it->begin},
                                    range.end,
                                    region.m_name });
                break;
            }
            else
            {
                regions.push_back({ beg_it->begin_coord(),
                                   end_it->end_coord(),
                                   region.m_name });
                auto end_coord = end_it->end_coord();

                // With empty begin and end matches (for example if the regexes
                // are /"\K/ and /(?=")/), that case can happen, and would
                // result in an infinite loop.
                if (end_coord == beg_it->begin_coord())
                {
                    kak_assert(beg_it->begin_coord() == beg_it->end_coord() and
                               end_it->begin_coord() == end_it->end_coord());
                    ++end_coord.column;
                }
                begin = find_next_begin(cache, end_coord);
            }
        }
        cache.timestamp = buf_timestamp;
        return regions;
    }
};

void setup_builtin_highlighters(HighlighterGroup& group)
{
    group.add_child({"tabulations"_str, std::make_unique<TabulationHighlighter>()});
    group.add_child({"unprintable"_str, make_highlighter(expand_unprintable)});
    group.add_child({"selections"_str,  make_highlighter(highlight_selections)});
}

void register_highlighters()
{
    HighlighterRegistry& registry = HighlighterRegistry::instance();

    registry.insert({
        "number_lines",
        { LineNumbersHighlighter::create,
          "Display line numbers \n"
          "Parameters: -relative, -hlcursor, -separator <separator text>\n" } });
    registry.insert({
        "show_matching",
        { create_matching_char_highlighter,
          "Apply the MatchingChar face to the char matching the one under the cursor" } });
    registry.insert({
        "show_whitespaces",
        { show_whitespaces_factory,
          "Display whitespaces using symbols \n"
          "Parameters: -tab <separator> -tabpad <separator> -lf <separator> -spc <separator> -nbsp <separator>\n" } });
    registry.insert({
        "fill",
        { create_fill_highlighter,
          "Fill the whole highlighted range with the given face" } });
    registry.insert({
        "regex",
        { RegexHighlighter::create,
          "Parameters: <regex> <capture num>:<face> <capture num>:<face>...\n"
          "Highlights the matches for captures from the regex with the given faces" } });
    registry.insert({
        "dynregex",
        { create_dynamic_regex_highlighter,
          "Parameters: <expr> <capture num>:<face> <capture num>:<face>...\n"
          "Evaluate expression at every redraw to gather a regex" } });
    registry.insert({
        "group",
        { create_highlighter_group,
          "Parameters: [-passes <passes>] <group name>\n"
          "Creates a named group that can contain other highlighters,\n"
          "<passes> is a flags(colorize|move|wrap) defaulting to colorize\n"
          "which specify what kind of highlighters can be put in the group" } });
    registry.insert({
        "flag_lines",
        { FlagLinesHighlighter::create,
          "Parameters: <face> <option name>\n"
          "Display flags specified in the line-spec option <option name> with <face>"} });
    registry.insert({
        "ranges",
        { RangesHighlighter::create,
          "Parameters: <option name>\n"
          "Use the range-specs option given as parameter to highlight buffer\n"
          "each spec is interpreted as a face to apply to the range\n" } });
    registry.insert({
        "replace-ranges",
        { ReplaceRangesHighlighter::create,
          "Parameters: <option name>\n"
          "Use the range-specs option given as parameter to highlight buffer\n"
          "each spec is interpreted as a display line to display in place of the range\n" } });
    registry.insert({
        "line",
        { create_line_highlighter,
          "Parameters: <value string> <face>\n"
          "Highlight the line given by evaluating <value string> with <face>" } });
    registry.insert({
        "column",
        { create_column_highlighter,
          "Parameters: <value string> <face>\n"
          "Highlight the column given by evaluating <value string> with <face>" } });
    registry.insert({
        "wrap",
        { WrapHighlighter::create,
          "Parameters: [-word] [-width <max_width>]\n"
          "Wrap lines to window width, or max_width if given and window is wider,\n"
          "wrap at word boundaries instead of codepoint boundaries if -word is given" } });
    registry.insert({
        "ref",
        { ReferenceHighlighter::create,
          "Parameters: [-passes <passes>] <path>\n"
          "Reference the highlighter at <path> in shared highlighters\n"
          "<passes> is a flags(colorize|move|wrap) defaulting to colorize\n"
          "which specify what kind of highlighters can be referenced" } });
    registry.insert({
        "regions",
        { RegionsHighlighter::create,
          "Parameters: [-default <default group>] [-match-capture] <name> {<region name> <begin> <end> <recurse>}..."
          "Split the highlighting into regions defined by the <begin>, <end> and <recurse> regex\n"
          "The region <region name> starts at <begin> match, end at <end> match that does not\n"
          "close a <recurse> match. In between region is the <default group>.\n"
          "Highlighting a region is done by adding highlighters into the different <region name> subgroups.\n"
          "If -match-capture is specified, then regions end/recurse matches are must have the same \1\n"
          "capture content as the begin to be considered"} });
}

}
