#ifndef selectors_hh_INCLUDED
#define selectors_hh_INCLUDED

#include "selection.hh"
#include "unicode.hh"
#include "editor.hh"

namespace Kakoune
{

enum WordType { Word, WORD };

template<WordType word_type>
Selection select_to_next_word(const Buffer& buffer,
                              const Selection& selection);
template<WordType word_type>
Selection select_to_next_word_end(const Buffer& buffer,
                                  const Selection& selection);
template<WordType word_type>
Selection select_to_previous_word(const Buffer& buffer,
const Selection& selection);

Selection select_line(const Buffer& buffer,
const Selection& selection);
Selection select_matching(const Buffer& buffer,
                          const Selection& selection);

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
constexpr bool operator&(ObjectFlags lhs, ObjectFlags rhs)
{ return (bool)((int)lhs & (int) rhs); }
constexpr ObjectFlags operator|(ObjectFlags lhs, ObjectFlags rhs)
{ return (ObjectFlags)((int)lhs | (int) rhs); }

template<WordType word_type>
Selection select_whole_word(const Buffer& buffer, const Selection& selection,
                            ObjectFlags flags);
Selection select_whole_sentence(const Buffer& buffer, const Selection& selection,
                                ObjectFlags flags);
Selection select_whole_paragraph(const Buffer& buffer, const Selection& selection,
                                 ObjectFlags flags);
Selection select_whole_indent(const Buffer& buffer, const Selection& selection,
                              ObjectFlags flags);
Selection select_whole_lines(const Buffer& buffer, const Selection& selection);
void select_whole_buffer(const Buffer& buffer, SelectionList& selections);
Selection trim_partial_lines(const Buffer& buffer, const Selection& selection);

enum Direction { Forward, Backward };

using MatchResults = boost::match_results<BufferIterator>;

static bool find_last_match(BufferIterator begin, const BufferIterator& end,
                            MatchResults& res, const Regex& regex)
{
    MatchResults matches;
    while (boost::regex_search(begin, end, matches, regex))
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
                          MatchResults& matches, const Regex& ex)
{
    if (direction == Forward)
        return (boost::regex_search(pos, buffer.end(), matches, ex) or
                boost::regex_search(buffer.begin(), pos, matches, ex));
    else
        return (find_last_match(buffer.begin(), pos, matches, ex) or
                find_last_match(pos, buffer.end(), matches, ex));
}

template<Direction direction, SelectMode mode>
void select_next_match(const Buffer& buffer, SelectionList& selections,
                                const Regex& regex)
{
    auto& sel = selections.main();
    auto begin = buffer.iterator_at(sel.last());
    auto end = begin;
    CaptureList captures;

    MatchResults matches;

    bool found = false;
    if ((found = find_match_in_buffer<direction>(buffer, utf8::next(begin), matches, regex)))
    {
        begin = matches[0].first;
        end   = matches[0].second;
        for (auto& match : matches)
            captures.emplace_back(match.first, match.second);
    }
    if (not found or begin == buffer.end())
        throw runtime_error("'" + regex.str() + "': no matches found");

    end = (begin == end) ? end : utf8::previous(end);
    if (direction == Backward)
        std::swap(begin, end);

    Selection res{begin.coord(), end.coord(), std::move(captures)};
    if (mode == SelectMode::Replace)
        selections = SelectionList{ std::move(res) };
    else if (mode == SelectMode::ReplaceMain)
        sel = std::move(res);
    else if (mode == SelectMode::Append)
    {
        selections.push_back(std::move(res));
        selections.set_main_index(selections.size() - 1);
    }
    selections.sort_and_merge_overlapping();
}

void select_all_matches(const Buffer& buffer, SelectionList& selections,
                                 const Regex& regex);

void split_selection(const Buffer& buffer, SelectionList& selections,
                              const Regex& separator_regex);

using CodepointPair = std::pair<Codepoint, Codepoint>;
Selection select_surrounding(const Buffer& buffer, const Selection& selection,
                             CodepointPair matching, int level, ObjectFlags flags);

}

#endif // selectors_hh_INCLUDED
