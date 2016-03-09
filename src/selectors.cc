#include "selectors.hh"

#include "optional.hh"
#include "string.hh"
#include "unit_tests.hh"

#include <algorithm>

namespace Kakoune
{

Selection select_line(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator first{buffer.iterator_at(selection.cursor()), buffer};
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
    Utf8Iterator it{buffer.iterator_at(selection.cursor()), buffer};
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

template<typename Iterator, typename Container>
Optional<Iterator> find_closing(Iterator pos, Iterator end,
                                Container opening, Container closing,
                                int init_level, bool nestable)
{
    const auto opening_len = opening.end() - opening.begin();
    const auto closing_len = closing.end() - closing.begin();

    int level = nestable ? init_level : 0;

    if (end - pos >= opening_len and
        std::equal(opening.begin(), opening.end(), pos))
        pos += opening_len;

    while (pos != end)
    {
        auto close = std::search(pos, end, closing.begin(), closing.end());
        if (close == end)
            return {};

        if (nestable)
        {
            for (auto open = pos; open != close; open += opening_len)
            {
                open = std::search(open, close, opening.begin(), opening.end());
                if (open == close)
                    break;
                ++level;
            }
        }

        pos = close + closing_len;
        if (level == 0)
            return pos-1;
        --level;
    }
    return {};
}

template<typename Iterator>
Optional<std::pair<Iterator, Iterator>>
find_surrounding(Iterator begin, Iterator end,
                 Iterator pos, StringView opening, StringView closing,
                 ObjectFlags flags, int init_level)
{
    const bool to_begin = flags & ObjectFlags::ToBegin;
    const bool to_end   = flags & ObjectFlags::ToEnd;
    const bool nestable = opening != closing;

    auto first = pos;
    if (to_begin)
    {
        using RevIt = std::reverse_iterator<Iterator>;
        auto res = find_closing(RevIt{pos+1}, RevIt{begin},
                                closing | reverse(), opening | reverse(),
                                init_level, nestable);
        if (not res)
            return {};

        first = res->base() - 1; 
    }

    auto last = pos;
    if (to_end)
    {
        auto res = find_closing(pos, end, opening, closing,
                                init_level, nestable);
        if (not res)
            return {};

        last = *res; 
    }

    if (flags & ObjectFlags::Inner)
    {
        if (to_begin and first != last)
            first += (int)opening.length();
        if (to_end and first != last)
            last -= (int)closing.length();
    }
    return to_end ? std::pair<Iterator, Iterator>{first, last}
                  : std::pair<Iterator, Iterator>{last, first};
}

template<typename Container, typename Iterator>
Optional<std::pair<Iterator, Iterator>>
find_surrounding(const Container& container, Iterator pos,
                 StringView opening, StringView closing,
                 ObjectFlags flags, int init_level)
{
    return find_surrounding(begin(container), end(container), pos,
                            opening, closing, flags, init_level);
}

Selection select_surrounding(const Buffer& buffer, const Selection& selection,
                             StringView opening, StringView closing, int level,
                             ObjectFlags flags)
{
    const bool nestable = opening != closing;
    auto pos = selection.cursor();
    if (not nestable or flags & ObjectFlags::Inner)
    {
        if (auto res = find_surrounding(buffer, buffer.iterator_at(pos),
                                        opening, closing, flags, level))
            return utf8_range(res->first, res->second);
        return selection;
    }

    auto c = buffer.byte_at(pos);
    if ((flags == ObjectFlags::ToBegin and c == opening) or
        (flags == ObjectFlags::ToEnd and c == closing))
        ++level;

    auto res = find_surrounding(buffer, buffer.iterator_at(pos),
                                opening, closing, flags, level);
    if (not res)
        return selection;

    Selection sel = utf8_range(res->first, res->second);

    if (flags == (ObjectFlags::ToBegin | ObjectFlags::ToEnd) and
        sel.min() == selection.min() and sel.max() == selection.max())
    {
        if (auto res_parent = find_surrounding(buffer, buffer.iterator_at(pos),
                                               opening, closing, flags, level+1))
            return utf8_range(res_parent->first, res_parent->second);
    }
    return sel;
}

Selection select_to(const Buffer& buffer, const Selection& selection,
                    Codepoint c, int count, bool inclusive)
{
    Utf8Iterator begin{buffer.iterator_at(selection.cursor()), buffer};
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
    Utf8Iterator begin{buffer.iterator_at(selection.cursor()), buffer};
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

Selection select_sentence(const Buffer& buffer, const Selection& selection, ObjectFlags flags)
{
    auto is_end_of_sentence = [](char c) {
        return c == '.' or c == ';' or c == '!' or c == '?';
    };

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
        skip_while_reverse(first, buffer.begin(),
                           [](Codepoint c){ return is_eol(c); });
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
                    skip_while(last, buffer.end(),
                               [](Codepoint c){ return is_eol(c); });
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
    return (flags & ObjectFlags::ToEnd) ? Selection{first.coord(), last.coord()}
                                        : Selection{last.coord(), first.coord()};
}

Selection select_indent(const Buffer& buffer, const Selection& selection, ObjectFlags flags)
{
    auto get_indent = [](StringView str, int tabstop) {
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
    };

    auto is_only_whitespaces = [](StringView str) {
        auto it = str.begin();
        skip_while(it, str.end(),
                   [](char c){ return c == ' ' or c == '\t' or c == '\n'; });
        return it == str.end();
    };

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

Selection select_argument(const Buffer& buffer, const Selection& selection,
                          int level, ObjectFlags flags)
{
    enum Class { None, Opening, Closing, Delimiter };
    auto classify = [](Codepoint c) {
        switch (c)
        {
        case '(': case '[': case '{': return Opening;
        case ')': case ']': case '}': return Closing;
        case ',': case ';': return Delimiter;
        default: return None;
        }
    };

    BufferIterator pos = buffer.iterator_at(selection.cursor());
    switch (classify(*pos))
    {
        //case Closing: if (pos+1 != buffer.end()) ++pos; break;
        case Opening:
        case Delimiter: if (pos != buffer.begin()) --pos; break;
        default: break;
    };

    bool first_arg = false;
    BufferIterator begin = pos;
    for (int lev = level; begin != buffer.begin(); --begin)
    {
        Class c = classify(*begin);
        if (c == Closing)
            ++lev;
        else if (c == Opening and (lev-- == 0))
        {
            first_arg = true;
            ++begin;
            break;
        }
        else if (c == Delimiter and lev == 0)
        {
            ++begin;
            break;
        }
    }

    bool last_arg = false;
    BufferIterator end = pos;
    for (int lev = level; end != buffer.end(); ++end)
    {
        Class c = classify(*end);
        if (c == Opening)
            ++lev;
        else if (end != pos and c == Closing and (lev-- == 0))
        {
            last_arg = true;
            --end;
            break;
        }
        else if (c == Delimiter and lev == 0)
        {
            // include whitespaces *after* the delimiter only for first argument
            if (first_arg and not (flags & ObjectFlags::Inner))
            {
                while (end + 1 != buffer.end() and is_blank(*(end+1)))
                    ++end;
            }
            break;
        }
    }

    if (flags & ObjectFlags::Inner)
    {
        if (not last_arg)
            --end;
        skip_while(begin, end, is_blank);
        skip_while_reverse(end, begin, is_blank);
    }
    // get starting delimiter for non inner last arg
    else if (not first_arg and last_arg)
        --begin;

    if (end == buffer.end())
        --end;

    if (flags & ObjectFlags::ToBegin and not (flags & ObjectFlags::ToEnd))
        return {pos.coord(), begin.coord()};
    return {(flags & ObjectFlags::ToBegin ? begin : pos).coord(), end.coord()};
}

Selection select_lines(const Buffer& buffer, const Selection& selection)
{
    ByteCoord anchor = selection.anchor();
    ByteCoord cursor  = selection.cursor();
    ByteCoord& to_line_start = anchor <= cursor ? anchor : cursor;
    ByteCoord& to_line_end = anchor <= cursor ? cursor : anchor;

    to_line_start.column = 0;
    to_line_end.column = buffer[to_line_end.line].length()-1;

    return target_eol({anchor, cursor});
}

Selection trim_partial_lines(const Buffer& buffer, const Selection& selection)
{
    ByteCoord anchor = selection.anchor();
    ByteCoord cursor  = selection.cursor();
    ByteCoord& to_line_start = anchor <= cursor ? anchor : cursor;
    ByteCoord& to_line_end = anchor <= cursor ? cursor : anchor;

    if (to_line_start.column != 0)
        to_line_start = to_line_start.line+1;
    if (to_line_end.column != buffer[to_line_end.line].length()-1)
    {
        if (to_line_end.line == 0)
            return selection;

        auto prev_line = to_line_end.line-1;
        to_line_end = ByteCoord{ prev_line, buffer[prev_line].length()-1 };
    }

    if (to_line_start > to_line_end)
        return selection;

    return target_eol({anchor, cursor});
}

void select_buffer(SelectionList& selections)
{
    auto& buffer = selections.buffer();
    selections = SelectionList{ buffer, target_eol({{0,0}, buffer.back_coord()}) };
}

using RegexIt = RegexIterator<BufferIterator>;

void select_all_matches(SelectionList& selections, const Regex& regex, unsigned capture)
{
    const unsigned mark_count = regex.mark_count();
    if (capture > mark_count)
        throw runtime_error("invalid capture number");

    Vector<Selection> result;
    auto& buffer = selections.buffer();
    for (auto& sel : selections)
    {
        auto sel_beg = buffer.iterator_at(sel.min());
        auto sel_end = utf8::next(buffer.iterator_at(sel.max()), buffer.end());
        const auto flags = match_flags(is_bol(sel_beg.coord()),
                                       is_eol(buffer, sel_end.coord()),
                                       is_eow(buffer, sel_end.coord()));
        RegexIt re_it(sel_beg, sel_end, regex, flags);
        RegexIt re_end;

        for (; re_it != re_end; ++re_it)
        {
            auto begin = ensure_char_start(buffer, (*re_it)[capture].first);
            if (begin == sel_end)
                continue;
            auto end = ensure_char_start(buffer, (*re_it)[capture].second);

            CaptureList captures;
            captures.reserve(mark_count);
            for (auto& match : *re_it)
                captures.push_back(buffer.string(match.first.coord(),
                                                 match.second.coord()));

            result.push_back(
                keep_direction({ begin.coord(),
                                 (begin == end ? end : utf8::previous(end, begin)).coord(),
                                 std::move(captures) }, sel));
        }
    }
    if (result.empty())
        throw runtime_error("nothing selected");

    // Avoid SelectionList::operator=(Vector<Selection>) as we know result is
    // already sorted and non overlapping.
    selections = SelectionList{buffer, std::move(result)};
}

void split_selections(SelectionList& selections, const Regex& regex, unsigned capture)
{
    if (capture > regex.mark_count())
        throw runtime_error("invalid capture number");

    Vector<Selection> result;
    auto& buffer = selections.buffer();
    auto buf_end = buffer.end();
    for (auto& sel : selections)
    {
        auto begin = buffer.iterator_at(sel.min());
        auto sel_end = utf8::next(buffer.iterator_at(sel.max()), buffer.end());
        const auto flags = match_flags(is_bol(begin.coord()),
                                       is_eol(buffer, sel_end.coord()),
                                       is_eow(buffer, sel_end.coord()));

        RegexIt re_it(begin, sel_end, regex, flags);
        RegexIt re_end;

        for (; re_it != re_end; ++re_it)
        {
            BufferIterator end = (*re_it)[capture].first;
            if (end == buf_end)
                continue;

            end = ensure_char_start(buffer, end);
            result.push_back(keep_direction({ begin.coord(), (begin == end) ? end.coord() : utf8::previous(end, begin).coord() }, sel));
            begin = ensure_char_start(buffer, (*re_it)[capture].second);
        }
        if (begin.coord() <= sel.max())
            result.push_back(keep_direction({ begin.coord(), sel.max() }, sel));
    }
    selections = std::move(result);
}

UnitTest test_find_surrounding{[]()
{
    StringView s("[salut { toi[] }]");
    {
        auto res = find_surrounding(s, s.begin() + 10, '{', '}',
                                    ObjectFlags::ToBegin | ObjectFlags::ToEnd, 0);
        kak_assert(res and StringView{res->first COMMA res->second+1} == "{ toi[] }");
    }
    {
        auto res = find_surrounding(s, s.begin() + 10, '[', ']',
                                    ObjectFlags::ToBegin | ObjectFlags::ToEnd | ObjectFlags::Inner, 0);
        kak_assert(res and StringView{res->first COMMA res->second+1} == "salut { toi[] }");
    }
    {
        auto res = find_surrounding(s, s.begin(), '[', ']',
                                    ObjectFlags::ToBegin | ObjectFlags::ToEnd, 0);
        kak_assert(res and StringView{res->first COMMA res->second+1} == "[salut { toi[] }]");
    }
    {
        auto res = find_surrounding(s, s.begin()+7, '{', '}',
                                    ObjectFlags::ToBegin | ObjectFlags::ToEnd, 0);
        kak_assert(res and StringView{res->first COMMA res->second+1} == "{ toi[] }");
    }
    {
        auto res = find_surrounding(s, s.begin() + 12, '[', ']',
                                    ObjectFlags::ToBegin | ObjectFlags::ToEnd | ObjectFlags::Inner, 0);
        kak_assert(res and StringView{res->first COMMA res->second+1} == "]");
    }
    {
        auto res = find_surrounding(s, s.begin() + 14, '[', ']',
                                    ObjectFlags::ToBegin | ObjectFlags::ToEnd, 0);
        kak_assert(res and StringView{res->first COMMA res->second+1} == "[salut { toi[] }]");
    }
    {
        auto res = find_surrounding(s, s.begin() + 1, '[', ']', ObjectFlags::ToBegin, 0);
        kak_assert(res and StringView{res->second COMMA res->first+1} == "[s");
    }
    s = "[]";
    {
        auto res = find_surrounding(s, s.begin() + 1, '[', ']', ObjectFlags::ToBegin | ObjectFlags::ToEnd, 0);
        kak_assert(res and StringView{res->first COMMA res->second+1} == "[]");
    }
    s = "[*][] hehe";
    {
        auto res = find_surrounding(s, s.begin() + 6, '[', ']', ObjectFlags::ToBegin, 0);
        kak_assert(not res);
    }
    s = "begin tchou begin tchaa end end";
    {
        auto res = find_surrounding(s, s.begin() + 6, "begin", "end",
                                    ObjectFlags::ToBegin | ObjectFlags::ToEnd, 0);
        kak_assert(res and StringView{res->first COMMA res->second+1} == s);
    }
}};

}
