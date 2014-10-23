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

inline void clear_selections(SelectionList& selections)
{
    for (auto& sel : selections)
        sel.anchor() = sel.cursor();
}

inline void flip_selections(SelectionList& selections)
{
    for (auto& sel : selections)
    {
        ByteCoord tmp = sel.anchor();
        sel.anchor() = sel.cursor();
        sel.cursor() = tmp;
    }
    selections.check_invariant();
}

inline void keep_selection(SelectionList& selections, int index)
{
    if (index < selections.size())
        selections = SelectionList{ selections.buffer(), std::move(selections[index]) };
    selections.check_invariant();
}

inline void remove_selection(SelectionList& selections, int index)
{
    if (selections.size() > 1 and index < selections.size())
    {
        selections.remove(index);
        size_t main_index = selections.main_index();
        if (index < main_index or main_index == selections.size())
            selections.set_main_index(main_index - 1);
    }
    selections.check_invariant();
}

using Utf8Iterator = utf8::iterator<BufferIterator, utf8::InvalidPolicy::Pass>;

inline Selection utf8_range(const Utf8Iterator& first, const Utf8Iterator& last)
{
    return {first.base().coord(), last.base().coord()};
}

template<WordType word_type>
Selection select_to_next_word(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator begin = buffer.iterator_at(selection.cursor());
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

    skip_while(end, buffer.end(), is_blank);

    return utf8_range(begin, end-1);
}

template<WordType word_type>
Selection select_to_next_word_end(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator begin = buffer.iterator_at(selection.cursor());
    if (begin+1 == buffer.end())
        return selection;
    if (categorize<word_type>(*begin) != categorize<word_type>(*(begin+1)))
        ++begin;

    skip_while(begin, buffer.end(), is_eol);
    if (begin == buffer.end())
        return selection;
    Utf8Iterator end = begin;
    skip_while(end, buffer.end(), is_blank);

    if (word_type == Word and is_punctuation(*end))
        skip_while(end, buffer.end(), is_punctuation);
    else if (is_word<word_type>(*end))
        skip_while(end, buffer.end(), is_word<word_type>);

    return utf8_range(begin, end-1);
}

template<WordType word_type>
Selection select_to_previous_word(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator begin = buffer.iterator_at(selection.cursor());
    if (begin == buffer.begin())
        return selection;
    if (categorize<word_type>(*begin) != categorize<word_type>(*(begin-1)))
        --begin;

    skip_while_reverse(begin, buffer.begin(), is_eol);
    Utf8Iterator end = begin;
    skip_while_reverse(end, buffer.begin(), is_blank);

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

Selection select_to_eol(const Buffer& buffer, const Selection& selection);
Selection select_to_eol_reverse(const Buffer& buffer, const Selection& selection);

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
    Utf8Iterator first = buffer.iterator_at(selection.cursor());
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
                skip_while(last, buffer.end(), is_blank);
            --last;
        }
    }
    else if (not (flags & ObjectFlags::Inner))
    {
        if (flags & ObjectFlags::ToBegin)
        {
            skip_while_reverse(first, buffer.begin(), is_blank);
            if (not is_word<word_type>(*first))
                return selection;
            skip_while_reverse(first, buffer.begin(), is_word<word_type>);
            if (not is_word<word_type>(*first))
                ++first;
        }
        if (flags & ObjectFlags::ToEnd)
        {
            skip_while(last, buffer.end(), is_blank);
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

template<Direction direction>
Selection find_next_match(const Buffer& buffer, const Selection& sel, const Regex& regex)
{
    auto begin = buffer.iterator_at(direction == Backward ? sel.min() : sel.max());
    auto end = begin;

    CaptureList captures;
    MatchResults<BufferIterator> matches;
    bool found = false;
    auto pos = direction == Forward ? utf8::next(begin, buffer.end())
                                    : utf8::previous(begin, buffer.begin());
    if ((found = find_match_in_buffer<direction>(buffer, pos, matches, regex)))
    {
        begin = matches[0].first;
        end   = matches[0].second;
        for (auto& match : matches)
            captures.emplace_back(match.first, match.second);
    }
    if (not found or begin == buffer.end())
        throw runtime_error("'" + regex.str() + "': no matches found");

    end = (begin == end) ? end : utf8::previous(end, begin);
    if (direction == Backward)
        std::swap(begin, end);

    return {begin.coord(), end.coord(), std::move(captures)};
}

void select_all_matches(SelectionList& selections,
                        const Regex& regex);

void split_selections(SelectionList& selections,
                      const Regex& separator_regex);

using CodepointPair = std::pair<Codepoint, Codepoint>;
Selection select_surrounding(const Buffer& buffer, const Selection& selection,
                             CodepointPair matching, int level, ObjectFlags flags);

}

#endif // selectors_hh_INCLUDED
