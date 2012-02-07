#ifndef selectors_hh_INCLUDED
#define selectors_hh_INCLUDED

#include "selection.hh"

namespace Kakoune
{

Selection select_to_next_word(const Selection& selection);
Selection select_to_next_word_end(const Selection& selection);
Selection select_to_previous_word(const Selection& selection);
Selection select_to_next_WORD(const Selection& selection);
Selection select_to_next_WORD_end(const Selection& selection);
Selection select_to_previous_WORD(const Selection& selection);
Selection select_line(const Selection& selection);
Selection select_matching(const Selection& selection);
Selection select_surrounding(const Selection& selection,
                             const std::pair<char, char>& matching,
                              bool inside);

Selection select_to(const Selection& selection,
                    char c, int count, bool inclusive);
Selection select_to_reverse(const Selection& selection,
                            char c, int count, bool inclusive);

Selection select_to_eol(const Selection& selection);
Selection select_to_eol_reverse(const Selection& selection);

Selection select_whole_lines(const Selection& selection);
Selection select_whole_buffer(const Selection& selection);

Selection select_next_match(const Selection& selection,
                            const std::string& regex);

SelectionList select_all_matches(const Selection& selection,
                                 const std::string& regex);

SelectionList split_selection(const Selection& selection,
                              const std::string& separator_regex);

}

#endif // selectors_hh_INCLUDED
