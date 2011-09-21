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

}

#endif // selectors_hh_INCLUDED
