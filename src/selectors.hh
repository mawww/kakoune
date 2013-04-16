#ifndef selectors_hh_INCLUDED
#define selectors_hh_INCLUDED

#include "selection.hh"
#include "unicode.hh"

namespace Kakoune
{

template<bool punctuation_is_word>
Selection select_to_next_word(const Selection& selection);
template<bool punctuation_is_word>
Selection select_to_next_word_end(const Selection& selection);
template<bool punctuation_is_word>
Selection select_to_previous_word(const Selection& selection);

Selection select_line(const Selection& selection);
Selection select_matching(const Selection& selection);

Selection select_to(const Selection& selection,
                    Codepoint c, int count, bool inclusive);
Selection select_to_reverse(const Selection& selection,
                            Codepoint c, int count, bool inclusive);

Selection select_to_eol(const Selection& selection);
Selection select_to_eol_reverse(const Selection& selection);

template<bool punctuation_is_word>
Selection select_whole_word(const Selection& selection, bool inner);
Selection select_whole_lines(const Selection& selection);
Selection select_whole_buffer(const Selection& selection);
Selection trim_partial_lines(const Selection& selection);

template<bool forward>
Selection select_next_match(const Selection& selection, const Regex& regex);

SelectionList select_all_matches(const Selection& selection,
                                 const Regex& regex);

SelectionList split_selection(const Selection& selection,
                              const Regex& separator_regex);

enum class SurroundFlags
{
    ToBegin = 1,
    ToEnd   = 2,
    Inner   = 4
};
constexpr bool operator&(SurroundFlags lhs, SurroundFlags rhs)
{ return (bool)((int)lhs & (int) rhs); }
constexpr SurroundFlags operator|(SurroundFlags lhs, SurroundFlags rhs)
{ return (SurroundFlags)((int)lhs | (int) rhs); }

using CodepointPair = std::pair<Codepoint, Codepoint>;
Selection select_surrounding(const Selection& selection,
                             const CodepointPair& matching,
                             SurroundFlags flags);

}

#endif // selectors_hh_INCLUDED
