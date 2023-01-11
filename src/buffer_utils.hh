#ifndef buffer_utils_hh_INCLUDED
#define buffer_utils_hh_INCLUDED

#include "buffer.hh"
#include "selection.hh"

#include "utf8_iterator.hh"
#include "unicode.hh"

namespace Kakoune
{

inline String content(const Buffer& buffer, const Selection& range)
{
    return buffer.string(range.min(), buffer.char_next(range.max()));
}

inline BufferCoord erase(Buffer& buffer, const Selection& range)
{
    return buffer.erase(range.min(), buffer.char_next(range.max()));
}

void replace(Buffer& buffer, ArrayView<BufferRange> ranges, ConstArrayView<String> strings);

inline CharCount char_length(const Buffer& buffer, const Selection& range)
{
    return utf8::distance(buffer.iterator_at(range.min()),
                          buffer.iterator_at(buffer.char_next(range.max())));
}

inline CharCount char_length(const Buffer& buffer, const BufferCoord& begin, const BufferCoord& end)
{
    return utf8::distance(buffer.iterator_at(begin), buffer.iterator_at(end));
}

inline ColumnCount column_length(const Buffer& buffer, const BufferCoord& begin, const BufferCoord& end)
{
    return utf8::column_distance(buffer.iterator_at(begin), buffer.iterator_at(end));
}

inline bool is_bol(BufferCoord coord)
{
    return coord.column == 0;
}

inline bool is_eol(const Buffer& buffer, BufferCoord coord)
{
    return buffer.is_end(coord) or buffer[coord.line].length() == coord.column+1;
}

inline bool is_bow(const Buffer& buffer, BufferCoord coord)
{
    auto it = utf8::iterator<BufferIterator>(buffer.iterator_at(coord), buffer);
    if (coord == BufferCoord{0,0})
        return is_word(*it);

    return not is_word(*(it-1)) and is_word(*it);
}

inline bool is_eow(const Buffer& buffer, BufferCoord coord)
{
    if (buffer.is_end(coord) or coord == BufferCoord{0,0})
        return false;

    auto it = utf8::iterator<BufferIterator>(buffer.iterator_at(coord), buffer);
    return is_word(*(it-1)) and not is_word(*it);
}

ColumnCount get_column(const Buffer& buffer, ColumnCount tabstop, BufferCoord coord);
ColumnCount column_length(const Buffer& buffer, ColumnCount tabstop, LineCount line);

ByteCount get_byte_to_column(const Buffer& buffer, ColumnCount tabstop,
                             DisplayCoord coord);

Buffer* create_fifo_buffer(String name, int fd, Buffer::Flags flags, bool scroll = false);
Buffer* create_buffer_from_string(String name, Buffer::Flags flags, StringView data);
Buffer* open_file_buffer(StringView filename,
                         Buffer::Flags flags = Buffer::Flags::None);
Buffer* open_or_create_file_buffer(StringView filename,
                                   Buffer::Flags flags = Buffer::Flags::None);
void reload_file_buffer(Buffer& buffer);

void write_to_debug_buffer(StringView str);

Vector<String> history_as_strings(const Vector<Buffer::HistoryNode>& history);
Vector<String> undo_group_as_strings(const Buffer::UndoGroup& undo_group);

String generate_buffer_name(StringView pattern);

}

#endif // buffer_utils_hh_INCLUDED
