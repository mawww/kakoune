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

CharCount get_column(const Buffer& buffer,
                     CharCount tabstop, ByteCoord coord);

Buffer* create_fifo_buffer(String name, int fd, bool scroll = false);

}

#endif // buffer_utils_hh_INCLUDED

