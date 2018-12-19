#include "selectors.hh"

#include "buffer_utils.hh"
#include "context.hh"
#include "flags.hh"
#include "optional.hh"
#include "option_types.hh"
#include "regex.hh"
#include "string.hh"
#include "unicode.hh"
#include "unit_tests.hh"
#include "utf8_iterator.hh"

#include <algorithm>

namespace Kakoune
{

using Utf8Iterator = utf8::iterator<BufferIterator>;

namespace
{

Selection target_eol(Selection sel)
{
    sel.cursor().target = INT_MAX;
    return sel;
}

Selection utf8_range(const BufferIterator& first, const BufferIterator& last)
{
    return {first.coord(), last.coord()};
}

Selection utf8_range(const Utf8Iterator& first, const Utf8Iterator& last)
{
    return {first.base().coord(), last.base().coord()};
}

ConstArrayView<Codepoint> get_extra_word_chars(const Context& context)
{
    return context.options()["extra_word_chars"].get<Vector<Codepoint, MemoryDomain::Options>>();
}

}

template<WordType word_type>
Optional<Selection>
select_to_next_word(const Context& context, const Selection& selection)
{
    auto extra_word_chars = get_extra_word_chars(context);
    auto& buffer = context.buffer();
    Utf8Iterator begin{buffer.iterator_at(selection.cursor()), buffer};
    if (begin+1 == buffer.end())
        return {};
    if (categorize<word_type>(*begin, extra_word_chars) !=
        categorize<word_type>(*(begin+1), extra_word_chars))
        ++begin;

    if (not skip_while(begin, buffer.end(),
                       [](Codepoint c) { return is_eol(c); }))
        return {};
    Utf8Iterator end = begin+1;

    auto is_word = [&](Codepoint c) { return Kakoune::is_word<word_type>(c, extra_word_chars); };
    auto is_punctuation = [&](Codepoint c) { return Kakoune::is_punctuation(c, extra_word_chars); };

    if (is_word(*begin))
        skip_while(end, buffer.end(), is_word);
    else if (is_punctuation(*begin))
        skip_while(end, buffer.end(), is_punctuation);

    skip_while(end, buffer.end(), is_horizontal_blank);

    return utf8_range(begin, end-1);
}
template Optional<Selection> select_to_next_word<WordType::Word>(const Context&, const Selection&);
template Optional<Selection> select_to_next_word<WordType::WORD>(const Context&, const Selection&);

template<WordType word_type>
Optional<Selection>
select_to_next_word_end(const Context& context, const Selection& selection)
{
    auto extra_word_chars = get_extra_word_chars(context);
    auto& buffer = context.buffer();
    Utf8Iterator begin{buffer.iterator_at(selection.cursor()), buffer};
    if (begin+1 == buffer.end())
        return {};
    if (categorize<word_type>(*begin, extra_word_chars) !=
        categorize<word_type>(*(begin+1), extra_word_chars))
        ++begin;

    if (not skip_while(begin, buffer.end(),
                       [](Codepoint c) { return is_eol(c); }))
        return {};
    Utf8Iterator end = begin;
    skip_while(end, buffer.end(), is_horizontal_blank);

    auto is_word = [&](Codepoint c) { return Kakoune::is_word<word_type>(c, extra_word_chars); };
    auto is_punctuation = [&](Codepoint c) { return Kakoune::is_punctuation(c, extra_word_chars); };

    if (is_word(*end))
        skip_while(end, buffer.end(), is_word);
    else if (is_punctuation(*end))
        skip_while(end, buffer.end(), is_punctuation);

    return utf8_range(begin, end-1);
}
template Optional<Selection> select_to_next_word_end<WordType::Word>(const Context&, const Selection&);
template Optional<Selection> select_to_next_word_end<WordType::WORD>(const Context&, const Selection&);

template<WordType word_type>
Optional<Selection>
select_to_previous_word(const Context& context, const Selection& selection)
{
    auto extra_word_chars = get_extra_word_chars(context);
    auto& buffer = context.buffer();
    Utf8Iterator begin{buffer.iterator_at(selection.cursor()), buffer};
    if (begin == buffer.begin())
        return {};
    if (categorize<word_type>(*begin, extra_word_chars) !=
        categorize<word_type>(*(begin-1), extra_word_chars))
        --begin;

    skip_while_reverse(begin, buffer.begin(), [](Codepoint c){ return is_eol(c); });
    Utf8Iterator end = begin;

    auto is_word = [&](Codepoint c) { return Kakoune::is_word<word_type>(c, extra_word_chars); };
    auto is_punctuation = [&](Codepoint c) { return Kakoune::is_punctuation(c, extra_word_chars); };

    bool with_end = skip_while_reverse(end, buffer.begin(), is_horizontal_blank);
    if (is_word(*end))
        with_end = skip_while_reverse(end, buffer.begin(), is_word);
    else if (is_punctuation(*end))
        with_end = skip_while_reverse(end, buffer.begin(), is_punctuation);

    return utf8_range(begin, with_end ? end : end+1);
}
template Optional<Selection> select_to_previous_word<WordType::Word>(const Context&, const Selection&);
template Optional<Selection> select_to_previous_word<WordType::WORD>(const Context&, const Selection&);

template<WordType word_type>
Optional<Selection>
select_word(const Context& context, const Selection& selection,
            int count, ObjectFlags flags)
{
    auto extra_word_chars = get_extra_word_chars(context);
    auto& buffer = context.buffer();

    auto is_word = [&](Codepoint c) { return Kakoune::is_word<word_type>(c, extra_word_chars); };

    Utf8Iterator first{buffer.iterator_at(selection.cursor()), buffer};
    if (not is_word(*first))
        return {};

    Utf8Iterator last = first;
    if (flags & ObjectFlags::ToBegin)
    {
        skip_while_reverse(first, buffer.begin(), is_word);
        if (not is_word(*first))
            ++first;
    }
    if (flags & ObjectFlags::ToEnd)
    {
        skip_while(last, buffer.end(), is_word);
        if (not (flags & ObjectFlags::Inner))
            skip_while(last, buffer.end(), is_horizontal_blank);
        --last;
    }
    return (flags & ObjectFlags::ToEnd) ? utf8_range(first, last)
                                        : utf8_range(last, first);
}
template Optional<Selection> select_word<WordType::Word>(const Context&, const Selection&, int, ObjectFlags);
template Optional<Selection> select_word<WordType::WORD>(const Context&, const Selection&, int, ObjectFlags);

Optional<Selection>
select_line(const Context& context, const Selection& selection)
{
    auto& buffer = context.buffer();
    auto line = selection.cursor().line;
    // Next line if line fully selected
    if (selection.anchor() <= BufferCoord{line, 0_byte} and
        selection.cursor() == BufferCoord{line, buffer[line].length() - 1} and
        line != buffer.line_count() - 1)
        ++line;
    return target_eol({{line, 0_byte}, {line, buffer[line].length() - 1}});
}

template<bool only_move>
Optional<Selection>
select_to_line_end(const Context& context, const Selection& selection)
{
    auto& buffer = context.buffer();
    BufferCoord begin = selection.cursor();
    LineCount line = begin.line;
    BufferCoord end = utf8::previous(buffer.iterator_at({line, buffer[line].length() - 1}),
                                     buffer.iterator_at(line)).coord();
    if (end < begin) // Do not go backward when cursor is on eol
        end = begin;
    return target_eol({only_move ? end : begin, end});
}
template Optional<Selection> select_to_line_end<false>(const Context&, const Selection&);
template Optional<Selection> select_to_line_end<true>(const Context&, const Selection&);

template<bool only_move>
Optional<Selection>
select_to_line_begin(const Context&, const Selection& selection)
{
    BufferCoord begin = selection.cursor();
    BufferCoord end = begin.line;
    return Selection{only_move ? end : begin, end};
}
template Optional<Selection> select_to_line_begin<false>(const Context&, const Selection&);
template Optional<Selection> select_to_line_begin<true>(const Context&, const Selection&);

Optional<Selection>
select_to_first_non_blank(const Context& context, const Selection& selection)
{
    auto& buffer = context.buffer();
    auto it = buffer.iterator_at(selection.cursor().line);
    skip_while(it, buffer.iterator_at(selection.cursor().line+1),
               is_horizontal_blank);
    return {it.coord()};
}

template<bool forward>
Optional<Selection>
select_matching(const Context& context, const Selection& selection)
{
    auto& buffer = context.buffer();
    auto& matching_pairs = context.options()["matching_pairs"].get<Vector<Codepoint, MemoryDomain::Options>>();
    Utf8Iterator it{buffer.iterator_at(selection.cursor()), buffer};
    auto match = matching_pairs.end();

    if (forward) while (it != buffer.end())
    {
        match = find(matching_pairs, *it);
        if (match != matching_pairs.end())
            break;
        ++it;
    }
    else while (true)
    {
        match = find(matching_pairs, *it);
        if (match != matching_pairs.end()
            or it == buffer.begin())
            break;
        --it;
    }

    if (match == matching_pairs.end())
        return {};

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
    return {};
}
template Optional<Selection>
select_matching<true>(const Context& context, const Selection& selection);
template Optional<Selection>
select_matching<false>(const Context& context, const Selection& selection);

template<typename Iterator, typename Container>
Optional<std::pair<Iterator, Iterator>>
find_opening(Iterator pos, const Container& container,
             const Regex& opening, const Regex& closing,
             int level, bool nestable)
{
    MatchResults<Iterator> res;
    // When on the token of a non-nestable block, we want to consider it opening
    if (nestable and
        backward_regex_search(container.begin(), pos,
                              container.begin(), container.end(), res, closing) and
        res[0].second == pos)
        pos = res[0].first;

    using RegexIt = RegexIterator<Iterator, MatchDirection::Backward>;
    for (auto&& match : RegexIt{container.begin(), pos, container.begin(), container.end(), opening})
    {
        if (nestable)
        {
            for (auto m [[gnu::unused]] : RegexIt{match[0].second, pos, container.begin(), container.end(), closing})
                ++level;
        }

        if (not nestable or level == 0)
            return match[0];
        pos = match[0].first;
        --level;
    }
    return {};
}

template<typename Iterator, typename Container>
Optional<std::pair<Iterator, Iterator>>
find_closing(Iterator pos, const Container& container,
             const Regex& opening, const Regex& closing,
             int level, bool nestable)
{
    MatchResults<Iterator> res;
    if (regex_search(pos, container.end(), container.begin(), container.end(),
                     res, opening) and res[0].first == pos)
        pos = res[0].second;

    using RegexIt = RegexIterator<Iterator, MatchDirection::Forward>;
    for (auto match : RegexIt{pos, container.end(), container.begin(), container.end(), closing})
    {
        if (nestable)
        {
            for (auto m [[gnu::unused]] : RegexIt{pos, match[0].first, container.begin(), container.end(), opening})
                ++level;
        }

        if (not nestable or level == 0)
            return match[0];
        pos = match[0].second;
        --level;
    }
    return {};
}

template<typename Container, typename Iterator>
Optional<std::pair<Iterator, Iterator>>
find_surrounding(const Container& container, Iterator pos,
                 const Regex& opening, const Regex& closing,
                 ObjectFlags flags, int level)
{
    const bool nestable = opening != closing;
    auto first = pos;
    auto last = pos;
    if (flags & ObjectFlags::ToBegin)
    {
        if (auto res = find_opening(first+1, container, opening, closing, level, nestable))
        {
            first = (flags & ObjectFlags::Inner) ? res->second : res->first;
            if (flags & ObjectFlags::ToEnd) // ensure we find the matching end
            {
                last = res->first;
                level = 0;
            }
        }
        else
            return {};
    }
    if (flags & ObjectFlags::ToEnd)
    {
        if (auto res = find_closing(last, container, opening, closing, level, nestable))
            last = (flags & ObjectFlags::Inner) ? utf8::previous(res->first, container.begin())
                                                : utf8::previous(res->second, container.begin());
        else
            return {};
    }
    if (first > last)
        last = first;

    return std::pair<Iterator, Iterator>{first, last};
}

Optional<Selection>
select_surrounding(const Context& context, const Selection& selection,
                   const Regex& opening, const Regex& closing, int level,
                   ObjectFlags flags)
{
    auto& buffer = context.buffer();
    auto pos = buffer.iterator_at(selection.cursor());

    auto res = find_surrounding(buffer, pos, opening, closing, flags, level);

    // If the ends we're changing didn't move, find the parent
    if (res and not (flags & ObjectFlags::Inner) and
        (res->first.coord() == selection.min() or not (flags & ObjectFlags::ToBegin)) and
        (res->second.coord() == selection.max() or not (flags & ObjectFlags::ToEnd)))
        res = find_surrounding(buffer, pos, opening, closing, flags, level+1);

    if (res)
        return (flags & ObjectFlags::ToEnd) ? utf8_range(res->first, res->second)
                                            : utf8_range(res->second, res->first);
    return {};
}

Optional<Selection>
select_to(const Context& context, const Selection& selection,
          Codepoint c, int count, bool inclusive)
{
    auto& buffer = context.buffer();
    Utf8Iterator begin{buffer.iterator_at(selection.cursor()), buffer};
    Utf8Iterator end = begin;
    do
    {
        ++end;
        skip_while(end, buffer.end(), [c](Codepoint cur) { return cur != c; });
        if (end == buffer.end())
            return {};
    }
    while (--count > 0);

    return utf8_range(begin, inclusive ? end : end-1);
}

Optional<Selection>
select_to_reverse(const Context& context, const Selection& selection,
                  Codepoint c, int count, bool inclusive)
{
    auto& buffer = context.buffer();
    Utf8Iterator begin{buffer.iterator_at(selection.cursor()), buffer};
    Utf8Iterator end = begin;
    do
    {
        --end;
        if (skip_while_reverse(end, buffer.begin(),
                               [c](Codepoint cur) { return cur != c; }))
            return {};
    }
    while (--count > 0);

    return utf8_range(begin, inclusive ? end : end+1);
}

Optional<Selection>
select_number(const Context& context, const Selection& selection,
              int count, ObjectFlags flags)
{
    auto is_number = [&](char c) {
        return (c >= '0' and c <= '9') or
               (not (flags & ObjectFlags::Inner) and c == '.');
    };

    auto& buffer = context.buffer();
    BufferIterator first = buffer.iterator_at(selection.cursor());
    BufferIterator last = first;

    if (not is_number(*first) and *first != '-')
        return {};

    if (flags & ObjectFlags::ToBegin)
    {
        skip_while_reverse(first, buffer.begin(), is_number);
        if (not is_number(*first) and *first != '-' and
            first+1 != buffer.end())
            ++first;
    }

    if (flags & ObjectFlags::ToEnd)
    {
        if (*last == '-')
            ++last;
        skip_while(last, buffer.end(), is_number);
        if (last != buffer.begin())
            --last;
    }

    return (flags & ObjectFlags::ToEnd) ? Selection{first.coord(), last.coord()}
                                        : Selection{last.coord(), first.coord()};
}

Optional<Selection>
select_sentence(const Context& context, const Selection& selection,
                int count, ObjectFlags flags)
{
    auto is_end_of_sentence = [](char c) {
        return c == '.' or c == ';' or c == '!' or c == '?';
    };

    auto& buffer = context.buffer();
    BufferIterator first = buffer.iterator_at(selection.cursor());

    if (not (flags & ObjectFlags::ToEnd) and first != buffer.begin())
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

Optional<Selection>
select_paragraph(const Context& context, const Selection& selection,
                 int count, ObjectFlags flags)
{
    auto& buffer = context.buffer();
    BufferIterator first = buffer.iterator_at(selection.cursor());

    if (not (flags & ObjectFlags::ToEnd) and first.coord() > BufferCoord{0,1} and
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

Optional<Selection>
select_whitespaces(const Context& context, const Selection& selection,
                   int count, ObjectFlags flags)
{
    auto is_whitespace = [&](char c) {
        return c == ' ' or c == '\t' or
            (not (flags & ObjectFlags::Inner) and c == '\n');
    };
    auto& buffer = context.buffer();
    BufferIterator first = buffer.iterator_at(selection.cursor());
    BufferIterator last  = first;

    if (not is_whitespace(*first))
        return {};

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

Optional<Selection>
select_indent(const Context& context, const Selection& selection,
              int count, ObjectFlags flags)
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

    auto get_current_indent = [&](const Buffer& buffer, LineCount line, int tabstop)
    {
        for (auto l = line; l >= 0; --l)
            if (buffer[l] != "\n"_sv)
                return get_indent(buffer[l], tabstop);
        for (auto l = line+1; l < buffer.line_count(); ++l)
            if (buffer[l] != "\n"_sv)
                return get_indent(buffer[l], tabstop);
        return 0_char;
    };

    auto is_only_whitespaces = [](StringView str) {
        auto it = str.begin();
        skip_while(it, str.end(),
                   [](char c){ return c == ' ' or c == '\t' or c == '\n'; });
        return it == str.end();
    };

    const bool to_begin = flags & ObjectFlags::ToBegin;
    const bool to_end   = flags & ObjectFlags::ToEnd;

    auto& buffer = context.buffer();
    int tabstop = context.options()["tabstop"].get<int>();
    auto pos = selection.cursor();
    const LineCount line = pos.line;
    const auto indent = get_current_indent(buffer, line, tabstop);

    LineCount begin_line = line - 1;
    if (to_begin)
    {
        while (begin_line >= 0 and (buffer[begin_line] == "\n"_sv or
                                    get_indent(buffer[begin_line], tabstop) >= indent))
            --begin_line;
    }
    ++begin_line;
    LineCount end_line = line + 1;
    if (to_end)
    {
        const LineCount end = buffer.line_count();
        while (end_line < end and (buffer[end_line] == "\n"_sv or
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

    auto first = to_begin ? begin_line : pos;
    auto last = to_end ? BufferCoord{end_line, buffer[end_line].length() - 1} : pos;
    return to_end ? Selection{first, last} : Selection{last, first};
}

Optional<Selection>
select_argument(const Context& context, const Selection& selection,
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

    auto& buffer = context.buffer();
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
        return Selection{pos.coord(), begin.coord()};
    return Selection{(flags & ObjectFlags::ToBegin ? begin : pos).coord(),
                     end.coord()};
}

Optional<Selection>
select_lines(const Context& context, const Selection& selection)
{
    auto& buffer = context.buffer();
    BufferCoord anchor = selection.anchor();
    BufferCoord cursor  = selection.cursor();
    BufferCoord& to_line_start = anchor <= cursor ? anchor : cursor;
    BufferCoord& to_line_end = anchor <= cursor ? cursor : anchor;

    to_line_start.column = 0;
    to_line_end.column = buffer[to_line_end.line].length()-1;

    return target_eol({anchor, cursor});
}

Optional<Selection>
trim_partial_lines(const Context& context, const Selection& selection)
{
    auto& buffer = context.buffer();
    BufferCoord anchor = selection.anchor();
    BufferCoord cursor  = selection.cursor();
    BufferCoord& to_line_start = anchor <= cursor ? anchor : cursor;
    BufferCoord& to_line_end = anchor <= cursor ? cursor : anchor;

    if (to_line_start.column != 0)
        to_line_start = to_line_start.line+1;
    if (to_line_end.column != buffer[to_line_end.line].length()-1)
    {
        if (to_line_end.line == 0)
            return {};

        auto prev_line = to_line_end.line-1;
        to_line_end = BufferCoord{ prev_line, buffer[prev_line].length()-1 };
    }

    if (to_line_start > to_line_end)
        return {};

    return target_eol({anchor, cursor});
}

void select_buffer(SelectionList& selections)
{
    auto& buffer = selections.buffer();
    selections = SelectionList{ buffer, target_eol({{0,0}, buffer.back_coord()}) };
}

static RegexExecFlags
match_flags(const Buffer& buf, const BufferIterator& begin, const BufferIterator& end)
{
    return match_flags(is_bol(begin.coord()), is_eol(buf, end.coord()),
                       is_bow(buf, begin.coord()), is_eow(buf, end.coord()));
}

static bool find_next(const Buffer& buffer, const BufferIterator& pos,
                      MatchResults<BufferIterator>& matches,
                      const Regex& ex, bool& wrapped)
{
    if (pos != buffer.end() and
        regex_search(pos, buffer.end(), buffer.begin(), buffer.end(),
                     matches, ex, match_flags(buffer, pos, buffer.end())))
        return true;
    wrapped = true;
    return regex_search(buffer.begin(), buffer.end(), buffer.begin(), buffer.end(),
                        matches, ex, match_flags(buffer, buffer.begin(), buffer.end()));
}

static bool find_prev(const Buffer& buffer, const BufferIterator& pos,
                      MatchResults<BufferIterator>& matches,
                      const Regex& ex, bool& wrapped)
{
    if (pos != buffer.begin() and
        backward_regex_search(buffer.begin(), pos, buffer.begin(), buffer.end(),
                              matches, ex,
                              match_flags(buffer, buffer.begin(), pos) |
                              RegexExecFlags::NotInitialNull))
        return true;
    wrapped = true;
    return backward_regex_search(buffer.begin(), buffer.end(), buffer.begin(), buffer.end(),
                                 matches, ex,
                                 match_flags(buffer, buffer.begin(), buffer.end()) |
                                 RegexExecFlags::NotInitialNull);
}

template<MatchDirection direction>
Selection find_next_match(const Context& context, const Selection& sel, const Regex& regex, bool& wrapped)
{
    auto& buffer = context.buffer();
    MatchResults<BufferIterator> matches;
    auto pos = buffer.iterator_at(direction == MatchDirection::Backward ? sel.min() : sel.max());
    wrapped = false;
    const bool found = (direction == MatchDirection::Forward) ?
        find_next(buffer, utf8::next(pos, buffer.end()), matches, regex, wrapped)
      : find_prev(buffer, pos, matches, regex, wrapped);

    if (not found or matches[0].first == buffer.end())
        throw runtime_error(format("no matches found: '{}'", regex.str()));

    CaptureList captures;
    for (const auto& match : matches)
        captures.push_back(buffer.string(match.first.coord(), match.second.coord()));

    auto begin = matches[0].first, end = matches[0].second;
    end = (begin == end) ? end : utf8::previous(end, begin);
    if (direction == MatchDirection::Backward)
        std::swap(begin, end);

    return {begin.coord(), end.coord(), std::move(captures)};
}
template Selection find_next_match<MatchDirection::Forward>(const Context&, const Selection&, const Regex&, bool&);
template Selection find_next_match<MatchDirection::Backward>(const Context&, const Selection&, const Regex&, bool&);

void select_all_matches(SelectionList& selections, const Regex& regex, int capture)
{
    const int mark_count = (int)regex.mark_count();
    if (capture < 0 or capture > mark_count)
        throw runtime_error("invalid capture number");

    Vector<Selection> result;
    auto& buffer = selections.buffer();
    for (auto& sel : selections)
    {
        auto sel_beg = buffer.iterator_at(sel.min());
        auto sel_end = utf8::next(buffer.iterator_at(sel.max()), buffer.end());

        for (auto&& match : RegexIterator{sel_beg, sel_end, regex, match_flags(buffer, sel_beg, sel_end)})
        {
            auto begin = match[capture].first;
            if (begin == sel_end)
                continue;
            auto end = match[capture].second;

            CaptureList captures;
            captures.reserve(mark_count);
            for (const auto& submatch : match)
                captures.push_back(buffer.string(submatch.first.coord(),
                                                 submatch.second.coord()));

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

void split_selections(SelectionList& selections, const Regex& regex, int capture)
{
    if (capture < 0 or capture > (int)regex.mark_count())
        throw runtime_error("invalid capture number");

    Vector<Selection> result;
    auto& buffer = selections.buffer();
    auto buf_end = buffer.end();
    auto buf_begin = buffer.begin();
    for (auto& sel : selections)
    {
        auto begin = buffer.iterator_at(sel.min());
        auto sel_end = utf8::next(buffer.iterator_at(sel.max()), buffer.end());

        for (auto&& match : RegexIterator{begin, sel_end, regex, match_flags(buffer, begin, sel_end)})
        {
            BufferIterator end = match[capture].first;
            if (end == buf_end)
                continue;

            if (end != buf_begin)
            {
                auto sel_end = (begin == end) ? end : utf8::previous(end, begin);
                result.push_back(keep_direction({ begin.coord(), sel_end.coord() }, sel));
            }
            begin = match[capture].second;
        }
        if (begin.coord() <= sel.max())
            result.push_back(keep_direction({ begin.coord(), sel.max() }, sel));
    }
    if (result.empty())
        throw runtime_error("nothing selected");

    selections = std::move(result);
}

UnitTest test_find_surrounding{[]()
{
    StringView s = "{foo [bar { baz[] }]}";
    auto check_equal = [&](const char* pos, StringView opening, StringView closing,
                           ObjectFlags flags, int level, StringView expected) {
        auto res = find_surrounding(s, pos,
                                    Regex{"\\Q" + opening, RegexCompileFlags::Backward},
                                    Regex{"\\Q" + closing, RegexCompileFlags::Backward},
                                    flags, level);
        kak_assert(res);
        auto min = std::min(res->first, res->second),
             max = std::max(res->first, res->second);
        kak_assert(StringView{min, max+1} == expected);
    };

    check_equal(s.begin() + 13, '{', '}', ObjectFlags::ToBegin | ObjectFlags::ToEnd, 0, "{ baz[] }");
    check_equal(s.begin() + 13, '[', ']', ObjectFlags::ToBegin | ObjectFlags::ToEnd | ObjectFlags::Inner, 0, "bar { baz[] }");
    check_equal(s.begin() + 5, '[', ']', ObjectFlags::ToBegin | ObjectFlags::ToEnd, 0, "[bar { baz[] }]");
    check_equal(s.begin() + 10, '{', '}', ObjectFlags::ToBegin | ObjectFlags::ToEnd, 0, "{ baz[] }");
    check_equal(s.begin() + 16, '[', ']', ObjectFlags::ToBegin | ObjectFlags::ToEnd | ObjectFlags::Inner, 0, "]");
    check_equal(s.begin() + 18, '[', ']', ObjectFlags::ToBegin | ObjectFlags::ToEnd, 0, "[bar { baz[] }]");
    check_equal(s.begin() + 6, '[', ']', ObjectFlags::ToBegin, 0, "[b");

    s = "[*][] foo";
    kak_assert(not find_surrounding(s, s.begin() + 6,
                                    Regex{"\\Q[", RegexCompileFlags::Backward},
                                    Regex{"\\Q]", RegexCompileFlags::Backward},
                                    ObjectFlags::ToBegin, 0));

    s = "begin foo begin bar end end";
    check_equal(s.begin() + 6, "begin", "end", ObjectFlags::ToBegin | ObjectFlags::ToEnd, 0, s);
    check_equal(s.begin() + 22, "begin", "end", ObjectFlags::ToBegin | ObjectFlags::ToEnd, 0, "begin bar end");
}};

}
