#ifndef buffer_inl_h_INCLUDED
#define buffer_inl_h_INCLUDED

#include "assert.hh"

namespace Kakoune
{

inline char Buffer::byte_at(BufferCoord c) const
{
    kak_assert(c.line < line_count() and c.column < m_lines[c.line].length());
    return m_lines[c.line].content[c.column];
}

inline BufferCoord Buffer::next(BufferCoord coord) const
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

inline BufferCoord Buffer::prev(BufferCoord coord) const
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

inline ByteCount Buffer::distance(BufferCoord begin, BufferCoord end) const
{
    return offset(end) - offset(begin);
}

inline ByteCount Buffer::offset(BufferCoord c) const
{
    if (c.line == line_count())
        return m_lines.back().start + m_lines.back().length();
    return m_lines[c.line].start + c.column;
}

inline bool Buffer::is_valid(BufferCoord c) const
{
    return (c.line < line_count() and c.column < m_lines[c.line].length()) or
           (c.line == line_count() - 1 and c.column == m_lines.back().length()) or
           (c.line == line_count() and c.column == 0);
}

inline bool Buffer::is_end(BufferCoord c) const
{
    return c >= BufferCoord{line_count() - 1, m_lines.back().length()};
}

inline BufferIterator Buffer::begin() const
{
    return BufferIterator(*this, { 0_line, 0 });
}

inline BufferIterator Buffer::end() const
{
    if (m_lines.empty())
        return BufferIterator(*this, { 0_line, 0 });
    return BufferIterator(*this, { line_count()-1, m_lines.back().length() });
}

inline ByteCount Buffer::byte_count() const
{
    if (m_lines.empty())
        return 0;
    return m_lines.back().start + m_lines.back().length();
}

inline LineCount Buffer::line_count() const
{
    return LineCount(m_lines.size());
}

inline BufferIterator::BufferIterator(const Buffer& buffer, BufferCoord coord)
    : m_buffer(&buffer), m_coord(coord)
{
    kak_assert(m_buffer and m_buffer->is_valid(m_coord));
}

inline bool BufferIterator::operator==(const BufferIterator& iterator) const
{
    return (m_buffer == iterator.m_buffer and m_coord == iterator.m_coord);
}

inline bool BufferIterator::operator!=(const BufferIterator& iterator) const
{
    return (m_buffer != iterator.m_buffer or m_coord != iterator.m_coord);
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
    return *this = (*this + size);
}

inline BufferIterator& BufferIterator::operator-=(ByteCount size)
{
    return *this = (*this - size);
}

inline BufferIterator& BufferIterator::operator++()
{
    m_coord = m_buffer->next(m_coord);
    return *this;
}

inline BufferIterator& BufferIterator::operator--()
{
    m_coord = m_buffer->prev(m_coord);
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
