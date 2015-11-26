#ifndef selectors_hh_INCLUDED
#define selectors_hh_INCLUDED

#include "flags.hh"
#include "selection.hh"
#include "buffer_utils.hh"
#include "unicode.hh"
#include "utf8_iterator.hh"
#include "regex.hh"

namespace Kakoune
{

inline Selection keep_direction(Selection res, const Selection& ref)
{
    if ((res.cursor() < res.anchor()) != (ref.cursor() < ref.anchor()))
        std::swap<ByteCoord>(res.cursor(), res.anchor());
    return res;
}

inline Selection target_eol(Selection sel)
{
    sel.cursor().target = INT_MAX;
    return sel;
}

template<typename Iterator, typename EndIterator, typename T>
void skip_while(Iterator& it, const EndIterator& end, T condition)
{
    while (it != end and condition(*it))
        ++it;
}

template<typename Iterator, typename BeginIterator, typename T>
void skip_while_reverse(Iterator& it, const BeginIterator& begin, T condition)
{
    while (it != begin and condition(*it))
        --it;
}

using Utf8Iterator = utf8::iterator<BufferIterator, utf8::InvalidPolicy::Pass>;

inline Selection utf8_range(const Utf8Iterator& first, const Utf8Iterator& last)
{
    return {first.base().coord(), last.base().coord()};
}

template<WordType word_type>
Selection select_to_next_word(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator begin{buffer.iterator_at(selection.cursor()), buffer};
    if (begin+1 == buffer.end())
        return selection;
    if (categorize<word_type>(*begin) != categorize<word_type>(*(begin+1)))
        ++begin;

    skip_while(begin, buffer.end(), is_eol);
    if (begin == buffer.end())
        return selection;
    Utf8Iterator end = begin+1;

    if (word_type == Word and is_punctuation(*begin))
        skip_while(end, buffer.end(), is_punctuation);
    else if (is_word<word_type>(*begin))
        skip_while(end, buffer.end(), is_word<word_type>);

    skip_while(end, buffer.end(), is_horizontal_blank);

    return utf8_range(begin, end-1);
}

template<WordType word_type>
Selection select_to_next_word_end(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator begin{buffer.iterator_at(selection.cursor()), buffer};
    if (begin+1 == buffer.end())
        return selection;
    if (categorize<word_type>(*begin) != categorize<word_type>(*(begin+1)))
        ++begin;

    skip_while(begin, buffer.end(), is_eol);
    if (begin == buffer.end())
        return selection;
    Utf8Iterator end = begin;
    skip_while(end, buffer.end(), is_horizontal_blank);

    if (word_type == Word and is_punctuation(*end))
        skip_while(end, buffer.end(), is_punctuation);
    else if (is_word<word_type>(*end))
        skip_while(end, buffer.end(), is_word<word_type>);

    return utf8_range(begin, end-1);
}

template<WordType word_type>
Selection select_to_previous_word(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator begin{buffer.iterator_at(selection.cursor()), buffer};
    if (begin == buffer.begin())
        return selection;
    if (categorize<word_type>(*begin) != categorize<word_type>(*(begin-1)))
        --begin;

    skip_while_reverse(begin, buffer.begin(), is_eol);
    Utf8Iterator end = begin;
    skip_while_reverse(end, buffer.begin(), is_horizontal_blank);

    bool with_end = false;
    if (word_type == Word and is_punctuation(*end))
    {
        skip_while_reverse(end, buffer.begin(), is_punctuation);
        with_end = is_punctuation(*end);
    }
    else if (is_word<word_type>(*end))
    {
        skip_while_reverse(end, buffer.begin(), is_word<word_type>);
        with_end = is_word<word_type>(*end);
    }

    return utf8_range(begin, with_end ? end : end+1);
}

Selection select_line(const Buffer& buffer, const Selection& selection);
Selection select_matching(const Buffer& buffer, const Selection& selection);

Selection select_to(const Buffer& buffer, const Selection& selection,
                    Codepoint c, int count, bool inclusive);
Selection select_to_reverse(const Buffer& buffer, const Selection& selection,
                            Codepoint c, int count, bool inclusive);

template<bool only_move>
Selection select_to_line_end(const Buffer& buffer, const Selection& selection)
{
    ByteCoord begin = selection.cursor();
    LineCount line = begin.line;
    ByteCoord end = utf8::previous(buffer.iterator_at({line, buffer[line].length() - 1}),
                                   buffer.iterator_at(line)).coord();
    return target_eol({only_move ? end : begin, end});
}

template<bool only_move>
Selection select_to_line_begin(const Buffer& buffer, const Selection& selection)
{
    ByteCoord begin = selection.cursor();
    ByteCoord end = begin.line;
    return {only_move ? end : begin, end};
}

enum class ObjectFlags
{
    ToBegin = 1,
    ToEnd   = 2,
    Inner   = 4
};

template<> struct WithBitOps<ObjectFlags> : std::true_type {};

template<WordType word_type>
Selection select_word(const Buffer& buffer,
                      const Selection& selection,
                      ObjectFlags flags)
{
    Utf8Iterator first{buffer.iterator_at(selection.cursor()), buffer};
    Utf8Iterator last = first;
    if (is_word<word_type>(*first))
    {
        if (flags & ObjectFlags::ToBegin)
        {
            skip_while_reverse(first, buffer.begin(), is_word<word_type>);
            if (not is_word<word_type>(*first))
                ++first;
        }
        if (flags & ObjectFlags::ToEnd)
        {
            skip_while(last, buffer.end(), is_word<word_type>);
            if (not (flags & ObjectFlags::Inner))
                skip_while(last, buffer.end(), is_horizontal_blank);
            --last;
        }
    }
    else if (not (flags & ObjectFlags::Inner))
    {
        if (flags & ObjectFlags::ToBegin)
        {
            skip_while_reverse(first, buffer.begin(), is_horizontal_blank);
            if (not is_word<word_type>(*first))
                return selection;
            skip_while_reverse(first, buffer.begin(), is_word<word_type>);
            if (not is_word<word_type>(*first))
                ++first;
        }
        if (flags & ObjectFlags::ToEnd)
        {
            skip_while(last, buffer.end(), is_horizontal_blank);
            --last;
        }
    }
    return (flags & ObjectFlags::ToEnd) ? utf8_range(first, last)
                                        : utf8_range(last, first);
}

Selection select_number(const Buffer& buffer,
                        const Selection& selection,
                        ObjectFlags flags);

Selection select_sentence(const Buffer& buffer,
                          const Selection& selection,
                          ObjectFlags flags);

Selection select_paragraph(const Buffer& buffer,
                           const Selection& selection,
                           ObjectFlags flags);

Selection select_whitespaces(const Buffer& buffer,
                             const Selection& selection,
                             ObjectFlags flags);

Selection select_indent(const Buffer& buffer,
                        const Selection& selection,
                        ObjectFlags flags);

Selection select_argument(const Buffer& buffer,
                          const Selection& selection,
                          int level, ObjectFlags flags);

Selection select_lines(const Buffer& buffer, const Selection& selection);

Selection trim_partial_lines(const Buffer& buffer, const Selection& selection);

void select_buffer(SelectionList& selections);

enum Direction { Forward, Backward };

inline bool find_last_match(BufferIterator begin, const BufferIterator& end,
                            MatchResults<BufferIterator>& res,
                            const Regex& regex)
{
    MatchResults<BufferIterator> matches;
    while (regex_search(begin, end, matches, regex))
    {
        if (begin == matches[0].second)
            break;
        begin = matches[0].second;
        res.swap(matches);
    }
    return not res.empty();
}

template<Direction direction>
bool find_match_in_buffer(const Buffer& buffer, const BufferIterator pos,
                          MatchResults<BufferIterator>& matches,
                          const Regex& ex)
{
    if (direction == Forward)
        return (regex_search(pos, buffer.end(), matches, ex) or
                regex_search(buffer.begin(), buffer.end(), matches, ex));
    else
        return (find_last_match(buffer.begin(), pos, matches, ex) or
                find_last_match(buffer.begin(), buffer.end(), matches, ex));
}

inline BufferIterator ensure_char_start(const Buffer& buffer, const BufferIterator& it)
{
    return it != buffer.end() ?
        utf8::character_start(it, buffer.iterator_at(it.coord().line)) : it;
}

template<Direction direction>
Selection find_next_match(const Buffer& buffer, const Selection& sel, const Regex& regex)
{
    auto begin = buffer.iterator_at(direction == Backward ? sel.min() : sel.max());
    auto end = begin;

    CaptureList captures;
    MatchResults<BufferIterator> matches;
    bool found = false;
    auto pos = direction == Forward ? utf8::next(begin, buffer.end()) : begin;
    if ((found = find_match_in_buffer<direction>(buffer, pos, matches, regex)))
    {
        begin = ensure_char_start(buffer, matches[0].first);
        end = ensure_char_start(buffer, matches[0].second);
        for (auto& match : matches)
            captures.emplace_back(match.first, match.second);
    }
    if (not found or begin == buffer.end())
        throw runtime_error(format("'{}': no matches found", regex.str()));

    end = (begin == end) ? end : utf8::previous(end, begin);
    if (direction == Backward)
        std::swap(begin, end);

    return {begin.coord(), end.coord(), std::move(captures)};
}

void select_all_matches(SelectionList& selections, const Regex& regex, unsigned capture = 0);
void split_selections(SelectionList& selections, const Regex& separator_regex, unsigned capture = 0);

struct MatchingPair { Codepoint opening, closing; };
Selection select_surrounding(const Buffer& buffer, const Selection& selection,
                             MatchingPair matching, int level, ObjectFlags flags);

}

#endif // selectors_hh_INCLUDED
