#ifndef selectors_hh_INCLUDED
#define selectors_hh_INCLUDED

#include "selection.hh"

namespace Kakoune
{

template<bool punctuation_is_word>
SelectionAndCaptures select_to_next_word(const Selection& selection);
template<bool punctuation_is_word>
SelectionAndCaptures select_to_next_word_end(const Selection& selection);
template<bool punctuation_is_word>
SelectionAndCaptures select_to_previous_word(const Selection& selection);

SelectionAndCaptures select_line(const Selection& selection);
SelectionAndCaptures select_matching(const Selection& selection);
SelectionAndCaptures select_surrounding(const Selection& selection,
                                        const std::pair<char, char>& matching,
                                        bool inside);

SelectionAndCaptures select_to(const Selection& selection,
                               char c, int count, bool inclusive);
SelectionAndCaptures select_to_reverse(const Selection& selection,
                                       char c, int count, bool inclusive);

SelectionAndCaptures select_to_eol(const Selection& selection);
SelectionAndCaptures select_to_eol_reverse(const Selection& selection);

template<bool punctuation_is_word>
SelectionAndCaptures select_whole_word(const Selection& selection, bool inner);
SelectionAndCaptures select_whole_lines(const Selection& selection);
SelectionAndCaptures select_whole_buffer(const Selection& selection);

SelectionAndCaptures select_next_match(const Selection& selection,
                                       const std::string& regex);

SelectionAndCapturesList select_all_matches(const Selection& selection,
                                            const std::string& regex);

SelectionAndCapturesList split_selection(const Selection& selection,
                                         const std::string& separator_regex);

}

#endif // selectors_hh_INCLUDED
