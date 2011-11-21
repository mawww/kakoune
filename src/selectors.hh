#ifndef selectors_hh_INCLUDED
#define selectors_hh_INCLUDED

#include "window.hh"

namespace Kakoune
{

Selection select_to_next_word(const BufferIterator& cursor);
Selection select_to_next_word_end(const BufferIterator& cursor);
Selection select_to_previous_word(const BufferIterator& cursor);
Selection select_to_next_WORD(const BufferIterator& cursor);
Selection select_to_next_WORD_end(const BufferIterator& cursor);
Selection select_to_previous_WORD(const BufferIterator& cursor);
Selection select_line(const BufferIterator& cursor);
Selection select_matching(const BufferIterator& cursor);

Selection select_to(const BufferIterator& cursor, char c, int count, bool inclusive);
Selection select_to_reverse(const BufferIterator& cursor, char c, int count, bool inclusive);

Selection select_to_eol(const BufferIterator& cursor);
Selection select_to_eol_reverse(const BufferIterator& cursor);

SelectionList select_whole_lines(const Selection& selection);

Selection select_next_match(const BufferIterator& cursor,
                            const std::string& regex);

SelectionList select_all_matches(const Selection& selection,
                                 const std::string& regex);

SelectionList split_selection(const Selection& selection,
                              const std::string& separator_regex);

}

#endif // selectors_hh_INCLUDED
