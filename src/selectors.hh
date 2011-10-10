#ifndef selectors_hh_INCLUDED
#define selectors_hh_INCLUDED

#include "window.hh"

namespace Kakoune
{

Selection select_to_next_word(const BufferIterator& cursor);
Selection select_to_next_word_end(const BufferIterator& cursor);
Selection select_to_previous_word(const BufferIterator& cursor);
Selection select_line(const BufferIterator& cursor);
Selection move_select(Window& window, const BufferIterator& cursor, const WindowCoord& offset);
Selection select_matching(const BufferIterator& cursor);

Selection select_to(const BufferIterator& cursor, char c, int count, bool inclusive);
Selection select_to_reverse(const BufferIterator& cursor, char c, int count, bool inclusive);

Selection select_to_eol(const BufferIterator& cursor);
Selection select_to_eol_reverse(const BufferIterator& cursor);
}

#endif // selectors_hh_INCLUDED
