#include "selectors.hh"

#include "optional.hh"
#include "string.hh"

#include <algorithm>

namespace Kakoune
{

static Selection target_eol(Selection sel)
{
    sel.cursor().target = INT_MAX;
    return sel;
}

Selection select_line(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator first = buffer.iterator_at(selection.cursor());
    if (*first == '\n' and first + 1 != buffer.end())
        ++first;

    while (first != buffer.begin() and *(first - 1) != '\n')
        --first;

    Utf8Iterator last = first;
    while (last + 1 != buffer.end() and *last != '\n')
        ++last;
    return target_eol(utf8_range(first, last));
}

Selection select_matching(const Buffer& buffer, const Selection& selection)
{
    Vector<Codepoint> matching_pairs = { '(', ')', '{', '}', '[', ']', '<', '>' };
    Utf8Iterator it = buffer.iterator_at(selection.cursor());
    Vector<Codepoint>::iterator match = matching_pairs.end();
    while (not is_eol(*it))
    {
        match = std::find(matching_pairs.begin(), matching_pairs.end(), *it);
        if (match != matching_pairs.end())
            break;
        ++it;
    }
    if (match == matching_pairs.end())
        return selection;

    Utf8Iterator begin = it;

    if (((match - matching_pairs.begin()) % 2) == 0)
    {
        int level = 0;
        const Codepoint opening = *match;
        const Codepoint closing = *(match+1);
        while (it != buffer.end())
        {
            if (*it == opening)
                ++level;
            else if (*it == closing and --level == 0)
                return utf8_range(begin, it);
            ++it;
        }
    }
    else
    {
        int level = 0;
        const Codepoint opening = *(match-1);
        const Codepoint closing = *match;
        while (true)
        {
            if (*it == closing)
                ++level;
            else if (*it == opening and --level == 0)
                return utf8_range(begin, it);
            if (it == buffer.begin())
                break;
            --it;
        }
    }
    return selection;
}

static Optional<Selection> find_surrounding(const Buffer& buffer,
                                            ByteCoord coord,
                                            MatchingPair matching,
                                            ObjectFlags flags, int init_level)
{
    const bool to_begin = flags & ObjectFlags::ToBegin;
    const bool to_end   = flags & ObjectFlags::ToEnd;
    const bool nestable = matching.opening != matching.closing;
    auto pos = buffer.iterator_at(coord);
    Utf8Iterator first = pos;
    if (to_begin)
    {
        int level = nestable ? init_level : 0;
        while (first != buffer.begin())
        {
            if (nestable and first != pos and *first == matching.closing)
                ++level;
            else if (*first == matching.opening)
            {
                if (level == 0)
                    break;
                else
                    --level;
            }
            --first;
        }
        if (level != 0 or *first != matching.opening)
            return Optional<Selection>{};
    }

    Utf8Iterator last = pos;
    if (to_end)
    {
        int level = nestable ? init_level : 0;
        while (last != buffer.end())
        {
            if (nestable and last != pos and *last == matching.opening)
                ++level;
            else if (*last == matching.closing)
            {
                if (level == 0)
                    break;
                else
                    --level;
            }
            ++last;
        }
        if (level != 0 or last == buffer.end())
            return Optional<Selection>{};
    }

    if (flags & ObjectFlags::Inner)
    {
        if (to_begin and first != last)
            ++first;
        if (to_end and first != last)
            --last;
    }
    return to_end ? utf8_range(first, last) : utf8_range(last, first);
}

Selection select_surrounding(const Buffer& buffer, const Selection& selection,
                             MatchingPair matching, int level,
                             ObjectFlags flags)
{
    const bool nestable = matching.opening != matching.closing;
    auto pos = selection.cursor();
    if (not nestable or flags & ObjectFlags::Inner)
    {
        if (auto res = find_surrounding(buffer, pos, matching, flags, level))
            return *res;
        return selection;
    }

    auto c = buffer.byte_at(pos);
    if ((flags == ObjectFlags::ToBegin and c == matching.opening) or
        (flags == ObjectFlags::ToEnd and c == matching.closing))
        ++level;

    auto res = find_surrounding(buffer, pos, matching, flags, level);
    if (not res)
        return selection;

    if (flags == (ObjectFlags::ToBegin | ObjectFlags::ToEnd) and
        res->min() == selection.min() and res->max() == selection.max())
    {
        if (auto res_parent = find_surrounding(buffer, pos, matching, flags, level+1))
            return Selection{*res_parent};
    }
    return *res;
}

Selection select_to(const Buffer& buffer, const Selection& selection,
                    Codepoint c, int count, bool inclusive)
{
    Utf8Iterator begin = buffer.iterator_at(selection.cursor());
    Utf8Iterator end = begin;
    do
    {
        ++end;
        skip_while(end, buffer.end(), [c](Codepoint cur) { return cur != c; });
        if (end == buffer.end())
            return selection;
    }
    while (--count > 0);

    return utf8_range(begin, inclusive ? end : end-1);
}

Selection select_to_reverse(const Buffer& buffer, const Selection& selection,
                            Codepoint c, int count, bool inclusive)
{
    Utf8Iterator begin = buffer.iterator_at(selection.cursor());
    Utf8Iterator end = begin;
    do
    {
        --end;
        skip_while_reverse(end, buffer.begin(), [c](Codepoint cur) { return cur != c; });
        if (end == buffer.begin())
            return selection;
    }
    while (--count > 0);

    return utf8_range(begin, inclusive ? end : end+1);
}

Selection select_to_eol(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator begin = buffer.iterator_at(selection.cursor());
    Utf8Iterator end = begin;
    skip_while(end, buffer.end(), [](Codepoint cur) { return not is_eol(cur); });
    return target_eol(utf8_range(begin, end != begin ? end-1 : end));
}

Selection select_to_eol_reverse(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator begin = buffer.iterator_at(selection.cursor());
    Utf8Iterator end = begin - 1;
    skip_while_reverse(end, buffer.begin(), [](Codepoint cur) { return not is_eol(cur); });
    return utf8_range(begin, end == buffer.begin() ? end : end+1);
}

Selection select_number(const Buffer& buffer, const Selection& selection, ObjectFlags flags)
{
    auto is_number = [&](char c) {
        return (c >= '0' and c <= '9') or
               (not (flags & ObjectFlags::Inner) and c == '.');
    };

    BufferIterator first = buffer.iterator_at(selection.cursor());
    BufferIterator last = first;
    if (flags & ObjectFlags::ToBegin)
    {
        skip_while_reverse(first, buffer.begin(), is_number);
        if (not is_number(*first) and *first != '-' and
            first+1 != buffer.end())
            ++first;
    }

    if (flags & ObjectFlags::ToEnd)
    {
        skip_while(last, buffer.end(), is_number);
        --last;
    }

    return (flags & ObjectFlags::ToEnd) ? Selection{first.coord(), last.coord()}
                                        : Selection{last.coord(), first.coord()};
}

static bool is_end_of_sentence(char c)
{
    return c == '.' or c == ';' or c == '!' or c == '?';
}

Selection select_sentence(const Buffer& buffer, const Selection& selection, ObjectFlags flags)
{
    BufferIterator first = buffer.iterator_at(selection.cursor());

    if (not (flags & ObjectFlags::ToEnd))
    {
        BufferIterator prev_non_blank = first-1;
        skip_while_reverse(prev_non_blank, buffer.begin(),
                           [](char c) { return is_horizontal_blank(c) or is_eol(c); });
        if (is_end_of_sentence(*prev_non_blank))
            first = prev_non_blank;
    }

    BufferIterator last = first;

    if (flags & ObjectFlags::ToBegin)
    {
        bool saw_non_blank = false;
        while (first != buffer.begin())
        {
            char cur = *first;
            char prev = *(first-1);
            if (not is_horizontal_blank(cur))
                saw_non_blank = true;
            if (is_eol(prev) and is_eol(cur))
            {
                ++first;
                break;
            }
            else if (is_end_of_sentence(prev))
            {
                if (saw_non_blank)
                    break;
                else if (flags & ObjectFlags::ToEnd)
                    last = first-1;
            }
            --first;
        }
        skip_while(first, buffer.end(), is_horizontal_blank);
    }
    if (flags & ObjectFlags::ToEnd)
    {
        while (last != buffer.end())
        {
            char cur = *last;
            if (is_end_of_sentence(cur) or
                (is_eol(cur) and (last+1 == buffer.end() or is_eol(*(last+1)))))
                break;
            ++last;
        }
        if (not (flags & ObjectFlags::Inner) and last != buffer.end())
        {
            ++last;
            skip_while(last, buffer.end(), is_horizontal_blank);
            --last;
        }
    }
    return (flags & ObjectFlags::ToEnd) ? Selection{first.coord(), last.coord()}
                                        : Selection{last.coord(), first.coord()};
}

Selection select_paragraph(const Buffer& buffer, const Selection& selection, ObjectFlags flags)
{
    BufferIterator first = buffer.iterator_at(selection.cursor());

    if (not (flags & ObjectFlags::ToEnd) and first.coord() > ByteCoord{0,1} and
        *(first-1) == '\n' and *(first-2) == '\n')
        --first;
    else if ((flags & ObjectFlags::ToEnd) and
             first != buffer.begin() and (first+1) != buffer.end() and
             *(first-1) == '\n' and *first == '\n')
        ++first;

    BufferIterator last = first;

    if ((flags & ObjectFlags::ToBegin) and first != buffer.begin())
    {
        skip_while_reverse(first, buffer.begin(), is_eol);
        if (flags & ObjectFlags::ToEnd)
            last = first;
        while (first != buffer.begin())
        {
            char cur = *first;
            char prev = *(first-1);
            if (is_eol(prev) and is_eol(cur))
            {
                ++first;
                break;
            }
            --first;
        }
    }
    if (flags & ObjectFlags::ToEnd)
    {
        if (last != buffer.end() and is_eol(*last))
            ++last;
        while (last != buffer.end())
        {
            if (last != buffer.begin() and is_eol(*last) and is_eol(*(last-1)))
            {
                if (not (flags & ObjectFlags::Inner))
                    skip_while(last, buffer.end(), is_eol);
                break;
            }
            ++last;
        }
        --last;
    }
    return (flags & ObjectFlags::ToEnd) ? Selection{first.coord(), last.coord()}
                                        : Selection{last.coord(), first.coord()};
}

Selection select_whitespaces(const Buffer& buffer, const Selection& selection, ObjectFlags flags)
{
    auto is_whitespace = [&](char c) {
        return c == ' ' or c == '\t' or
            (not (flags & ObjectFlags::Inner) and c == '\n');
    };
    BufferIterator first = buffer.iterator_at(selection.cursor());
    BufferIterator last  = first;
    if (flags & ObjectFlags::ToBegin)
    {
        if (is_whitespace(*first))
        {
            skip_while_reverse(first, buffer.begin(), is_whitespace);
            if (not is_whitespace(*first))
                ++first;
        }
    }
    if (flags & ObjectFlags::ToEnd)
    {
        if (is_whitespace(*last))
        {
            skip_while(last, buffer.end(), is_whitespace);
            --last;
        }
    }
    return (flags & ObjectFlags::ToEnd) ? utf8_range(first, last)
                                        : utf8_range(last, first);
}

static CharCount get_indent(StringView str, int tabstop)
{
    CharCount indent = 0;
    for (auto& c : str)
    {
    if (c == ' ')
        ++indent;
    else if (c =='\t')
        indent = (indent / tabstop + 1) * tabstop;
    else
        break;
    }
    return indent;
}

static bool is_only_whitespaces(StringView str)
{
    auto it = str.begin();
    skip_while(it, str.end(),
               [](char c){ return c == ' ' or c == '\t' or c == '\n'; });
    return it == str.end();
}

Selection select_indent(const Buffer& buffer, const Selection& selection, ObjectFlags flags)
{
    int tabstop = buffer.options()["tabstop"].get<int>();
    LineCount line = selection.cursor().line;
    auto indent = get_indent(buffer[line], tabstop);

    LineCount begin_line = line - 1;
    if (flags & ObjectFlags::ToBegin)
    {
        while (begin_line >= 0 and (buffer[begin_line] == StringView{"\n"} or
                                    get_indent(buffer[begin_line], tabstop) >= indent))
            --begin_line;
    }
    ++begin_line;
    LineCount end_line = line + 1;
    if (flags & ObjectFlags::ToEnd)
    {
        const LineCount end = buffer.line_count();
        while (end_line < end and (buffer[end_line] == StringView{"\n"} or
                                   get_indent(buffer[end_line], tabstop) >= indent))
            ++end_line;
    }
    --end_line;
    // remove only whitespaces lines in inner mode
    if (flags & ObjectFlags::Inner)
    {
        while (begin_line < end_line and
               is_only_whitespaces(buffer[begin_line]))
            ++begin_line;
        while (begin_line < end_line and
               is_only_whitespaces(buffer[end_line]))
            --end_line;
    }
    return Selection{begin_line, {end_line, buffer[end_line].length() - 1}};
}

Selection select_lines(const Buffer& buffer, const Selection& selection)
{
    // no need to be utf8 aware for is_eol as we only use \n as line seperator
    BufferIterator first = buffer.iterator_at(selection.anchor());
    BufferIterator last  = buffer.iterator_at(selection.cursor());
    BufferIterator& to_line_start = first <= last ? first : last;
    BufferIterator& to_line_end = first <= last ? last : first;

    --to_line_start;
    skip_while_reverse(to_line_start, buffer.begin(), [](char cur) { return not is_eol(cur); });
    if (is_eol(*to_line_start))
    {
        if (++to_line_start == buffer.end())
            --to_line_start;
    }

    skip_while(to_line_end, buffer.end(), [](char cur) { return not is_eol(cur); });
    if (to_line_end == buffer.end())
        --to_line_end;

    return target_eol({first.coord(), last.coord()});
}

Selection trim_partial_lines(const Buffer& buffer, const Selection& selection)
{
    // same as select_lines
    BufferIterator first = buffer.iterator_at(selection.anchor());
    BufferIterator last =  buffer.iterator_at(selection.cursor());
    BufferIterator& to_line_start = first <= last ? first : last;
    BufferIterator& to_line_end = first <= last ? last : first;

    while (to_line_start != buffer.begin() and *(to_line_start-1) != '\n')
        ++to_line_start;
    while (*(to_line_end+1) != '\n' and to_line_end != to_line_start)
        --to_line_end;

    return target_eol({first.coord(), last.coord()});
}

void select_buffer(SelectionList& selections)
{
    auto& buffer = selections.buffer();
    selections = SelectionList{ buffer, target_eol({{0,0}, buffer.back_coord()}) };
}

using RegexIt = RegexIterator<BufferIterator>;

void select_all_matches(SelectionList& selections, const Regex& regex)
{
    Vector<Selection> result;
    auto& buffer = selections.buffer();
    for (auto& sel : selections)
    {
        auto sel_end = utf8::next(buffer.iterator_at(sel.max()), buffer.end());
        RegexIt re_it(buffer.iterator_at(sel.min()), sel_end, regex);
        RegexIt re_end;

        for (; re_it != re_end; ++re_it)
        {
            auto begin = ensure_char_start(buffer, (*re_it)[0].first);
            auto end = ensure_char_start(buffer, (*re_it)[0].second);

            if (begin == sel_end)
                continue;

            CaptureList captures;
            for (auto& match : *re_it)
                captures.emplace_back(match.first, match.second);

            result.push_back(
                keep_direction({ begin.coord(),
                                 (begin == end ? end : utf8::previous(end, begin)).coord(),
                                 std::move(captures) }, sel));
        }
    }
    if (result.empty())
        throw runtime_error("nothing selected");
    selections = std::move(result);
}

void split_selections(SelectionList& selections, const Regex& regex)
{
    Vector<Selection> result;
    auto& buffer = selections.buffer();
    auto buf_end = buffer.end();
    for (auto& sel : selections)
    {
        auto begin = buffer.iterator_at(sel.min());
        auto sel_end = utf8::next(buffer.iterator_at(sel.max()), buffer.end());
        RegexIt re_it(begin, sel_end, regex);
        RegexIt re_end;

        for (; re_it != re_end; ++re_it)
        {
            BufferIterator end = (*re_it)[0].first;
            if (end == buf_end)
                continue;

            end = ensure_char_start(buffer, end);
            result.push_back(keep_direction({ begin.coord(), (begin == end) ? end.coord() : utf8::previous(end, begin).coord() }, sel));
            begin = ensure_char_start(buffer, (*re_it)[0].second);
        }
        if (begin.coord() <= sel.max())
            result.push_back(keep_direction({ begin.coord(), sel.max() }, sel));
    }
    selections = std::move(result);
}

}
