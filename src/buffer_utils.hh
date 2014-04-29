#ifndef buffer_utils_hh_INCLUDED
#define buffer_utils_hh_INCLUDED

#include "buffer.hh"
#include "selection.hh"

namespace Kakoune
{

inline String content(const Buffer& buffer, const Selection& range)
{
    return buffer.string(range.min(), buffer.char_next(range.max()));
}

inline BufferIterator erase(Buffer& buffer, const Selection& range)
{
    return buffer.erase(buffer.iterator_at(range.min()),
                        utf8::next(buffer.iterator_at(range.max())));
}

inline CharCount char_length(const Buffer& buffer, const Selection& range)
{
    return utf8::distance(buffer.iterator_at(range.min()),
                          utf8::next(buffer.iterator_at(range.max())));
}

inline void avoid_eol(const Buffer& buffer, BufferCoord& coord)
{
    const auto column = coord.column;
    const auto& line = buffer[coord.line];
    if (column != 0 and column == line.length() - 1)
        coord.column = line.byte_count_to(line.char_length() - 2);
}

inline void avoid_eol(const Buffer& buffer, Selection& sel)
{
    avoid_eol(buffer, sel.anchor());
    avoid_eol(buffer, sel.cursor());
}

CharCount get_column(const Buffer& buffer,
                     CharCount tabstop, BufferCoord coord);

Buffer* create_fifo_buffer(String name, int fd);

}

#endif // buffer_utils_hh_INCLUDED

