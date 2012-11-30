#ifndef selectors_hh_INCLUDED
#define selectors_hh_INCLUDED

#include "selection.hh"
#include "utf8.hh"

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

using CodepointPair = std::pair<Codepoint, Codepoint>;
Selection select_surrounding(const Selection& selection,
                             const CodepointPair& matching, bool inside);

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

Selection select_next_match(const Selection& selection,
                            const String& regex);

SelectionList select_all_matches(const Selection& selection,
                                 const String& regex);

SelectionList split_selection(const Selection& selection,
                              const String& separator_regex);

}

#endif // selectors_hh_INCLUDED
