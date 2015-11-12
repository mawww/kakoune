#ifndef buffer_inl_h_INCLUDED
#define buffer_inl_h_INCLUDED

#include "assert.hh"

namespace Kakoune
{

[[gnu::always_inline]]
inline char Buffer::byte_at(ByteCoord c) const
{
    kak_assert(c.line < line_count() and c.column < m_lines[c.line].length());
    return m_lines[c.line][c.column];
}

inline ByteCoord Buffer::next(ByteCoord coord) const
{
    if (coord.column < m_lines[coord.line].length() - 1)
        ++coord.column;
    else if (coord.line == m_lines.size() - 1)
        coord.column = m_lines.back().length();
    else
    {
        ++coord.line;
        coord.column = 0;
    }
    return coord;
}

inline ByteCoord Buffer::prev(ByteCoord coord) const
{
    if (coord.column == 0)
    {
        if (coord.line > 0)
            coord.column = m_lines[--coord.line].length() - 1;
    }
    else
       --coord.column;
    return coord;
}

inline ByteCount Buffer::distance(ByteCoord begin, ByteCoord end) const
{
    if (begin > end)
        return -distance(end, begin);
    if (begin.line == end.line)
        return end.column - begin.column;

    ByteCount res = m_lines[begin.line].length() - begin.column;
    for (LineCount l = begin.line+1; l < end.line; ++l)
        res += m_lines[l].length();
    res += end.column;
    return res;
}

inline bool Buffer::is_valid(ByteCoord c) const
{
    if (c.line < 0 or c.column < 0)
        return false;

    return (c.line < line_count() and c.column < m_lines[c.line].length()) or
           (c.line == line_count() - 1 and c.column == m_lines.back().length()) or
           (c.line == line_count() and c.column == 0);
}

inline bool Buffer::is_end(ByteCoord c) const
{
    return c >= end_coord();
}

inline BufferIterator Buffer::begin() const
{
    return {*this, { 0_line, 0 }};
}

inline BufferIterator Buffer::end() const
{
    return {*this, end_coord()};
}

[[gnu::always_inline]]
inline LineCount Buffer::line_count() const
{
    return LineCount(m_lines.size());
}

inline size_t Buffer::timestamp() const
{
    return m_changes.size();
}

inline ConstArrayView<Buffer::Change> Buffer::changes_since(size_t timestamp) const
{
    return { m_changes.data() + timestamp,
             m_changes.data() + m_changes.size() };
}

inline ByteCoord Buffer::back_coord() const
{
    return { line_count() - 1, m_lines.back().length() - 1 };
}

inline ByteCoord Buffer::end_coord() const
{
    return { line_count() - 1, m_lines.back().length() };
}

inline BufferIterator::BufferIterator(const Buffer& buffer, ByteCoord coord)
    : m_buffer(&buffer), m_coord(coord),
      m_line_length((*m_buffer)[m_coord.line].length()),
      m_last_line(buffer.line_count()-1)
{
    kak_assert(m_buffer and m_buffer->is_valid(m_coord));
}

inline bool BufferIterator::operator==(const BufferIterator& iterator) const
{
    kak_assert(m_buffer == iterator.m_buffer);
    return m_coord == iterator.m_coord;
}

inline bool BufferIterator::operator!=(const BufferIterator& iterator) const
{
    kak_assert(m_buffer == iterator.m_buffer);
    return m_coord != iterator.m_coord;
}

inline bool BufferIterator::operator<(const BufferIterator& iterator) const
{
    kak_assert(m_buffer == iterator.m_buffer);
    return (m_coord < iterator.m_coord);
}

inline bool BufferIterator::operator<=(const BufferIterator& iterator) const
{
    kak_assert(m_buffer == iterator.m_buffer);
    return (m_coord <= iterator.m_coord);
}

inline bool BufferIterator::operator>(const BufferIterator& iterator) const
{
    kak_assert(m_buffer == iterator.m_buffer);
    return (m_coord > iterator.m_coord);
}

inline bool BufferIterator::operator>=(const BufferIterator& iterator) const
{
    kak_assert(m_buffer == iterator.m_buffer);
    return (m_coord >= iterator.m_coord);
}

[[gnu::always_inline]]
inline char BufferIterator::operator*() const
{
    return m_buffer->byte_at(m_coord);
}

inline char BufferIterator::operator[](size_t n) const
{
    return m_buffer->byte_at(m_buffer->advance(m_coord, n));
}

inline size_t BufferIterator::operator-(const BufferIterator& iterator) const
{
    kak_assert(m_buffer == iterator.m_buffer);
    return (size_t)(int)m_buffer->distance(iterator.m_coord, m_coord);
}

inline BufferIterator BufferIterator::operator+(ByteCount size) const
{
    kak_assert(m_buffer);
    return { *m_buffer, m_buffer->advance(m_coord, size) };
}

inline BufferIterator BufferIterator::operator-(ByteCount size) const
{
    return { *m_buffer, m_buffer->advance(m_coord, -size) };
}

inline BufferIterator& BufferIterator::operator+=(ByteCount size)
{
    m_coord = m_buffer->advance(m_coord, size);
    m_line_length = m_buffer->line_storage(m_coord.line)->length;
    return *this;
}

inline BufferIterator& BufferIterator::operator-=(ByteCount size)
{
    m_coord = m_buffer->advance(m_coord, -size);
    m_line_length = m_buffer->line_storage(m_coord.line)->length;
    return *this;
}

inline BufferIterator& BufferIterator::operator++()
{
    if (++m_coord.column == m_line_length and m_coord.line != m_last_line)
    {
        ++m_coord.line;
        m_coord.column = 0;
        m_line_length = m_buffer->line_storage(m_coord.line)->length;
    }
    return *this;
}

inline BufferIterator& BufferIterator::operator--()
{
    if (m_coord.column == 0 and m_coord.line > 0)
    {
        m_line_length = m_buffer->line_storage(--m_coord.line)->length;
        m_coord.column = m_line_length - 1;
    }
    else
       --m_coord.column;
    return *this;
}

inline BufferIterator BufferIterator::operator++(int)
{
    BufferIterator save = *this;
    ++*this;
    return save;
}

inline BufferIterator BufferIterator::operator--(int)
{
    BufferIterator save = *this;
    --*this;
    return save;
}

}
#endif // buffer_inl_h_INCLUDED
