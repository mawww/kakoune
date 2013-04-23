#ifndef buffer_iterator_inl_h_INCLUDED
#define buffer_iterator_inl_h_INCLUDED

#include "assert.hh"

namespace Kakoune
{

inline BufferIterator::BufferIterator(const Buffer& buffer, BufferCoord coord)
    : m_buffer(&buffer), m_coord(coord)
{
    kak_assert(is_valid());
}

inline const Buffer& BufferIterator::buffer() const
{
    kak_assert(m_buffer);
    return *m_buffer;
}

inline bool BufferIterator::is_valid() const
{
    return m_buffer and m_buffer->is_valid(m_coord);
}

inline void BufferIterator::clamp(bool avoid_eol)
{
    kak_assert(m_buffer);
    m_coord = m_buffer->clamp(m_coord, avoid_eol);
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
    return m_buffer->m_lines[m_coord.line].content[m_coord.column];
}

inline ByteCount BufferIterator::offset() const
{
    kak_assert(m_buffer);
    return line() >= m_buffer->line_count() ?
        m_buffer->character_count() : m_buffer->m_lines[line()].start + column();
}

inline size_t BufferIterator::operator-(const BufferIterator& iterator) const
{
    kak_assert(m_buffer == iterator.m_buffer);
    return (size_t)(int)(offset() - iterator.offset());
}

inline BufferIterator BufferIterator::operator+(ByteCount size) const
{
    kak_assert(m_buffer);
    if (size >= 0)
    {
        ByteCount o = std::min(m_buffer->character_count(), offset() + size);
        for (LineCount i = line() + 1; i < m_buffer->line_count(); ++i)
        {
            if (m_buffer->m_lines[i].start > o)
                return BufferIterator(*m_buffer, { i-1, o - m_buffer->m_lines[i-1].start });
        }
        LineCount last_line = std::max(0_line, m_buffer->line_count() - 1);
        return BufferIterator(*m_buffer, { last_line, o - m_buffer->m_lines[last_line].start });
    }
    return operator-(-size);
}

inline BufferIterator BufferIterator::operator-(ByteCount size) const
{
    kak_assert(m_buffer);
    if (size >= 0)
    {
        ByteCount o = std::max(0_byte, offset() - size);
        for (LineCount i = line(); i >= 0; --i)
        {
            if (m_buffer->m_lines[i].start <= o)
                return BufferIterator(*m_buffer, { i, o - m_buffer->m_lines[i].start });
        }
        kak_assert(false);
    }
    return operator+(-size);
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
    if (m_coord.column < m_buffer->m_lines[m_coord.line].length() - 1)
        ++m_coord.column;
    else if (m_coord.line == m_buffer->m_lines.size() - 1)
        m_coord.column = m_buffer->m_lines.back().length();
    else
    {
        ++m_coord.line;
        m_coord.column = 0;
    }
    return *this;
}

inline BufferIterator& BufferIterator::operator--()
{
    if (column() == 0)
    {
        if (line() > 0)
        {
            --m_coord.line;
            m_coord.column = m_buffer->m_lines[m_coord.line].length() - 1;
        }
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

inline BufferIterator& BufferIterator::operator=(const BufferCoord& coord)
{
    m_coord = coord;
    kak_assert(is_valid());
    return *this;
}

inline bool BufferIterator::is_begin() const
{
    kak_assert(m_buffer);
    return m_coord.line == 0 and m_coord.column == 0;
}

inline bool BufferIterator::is_end() const
{
    kak_assert(m_buffer);
    if (m_coord.line == m_buffer->line_count())
    {
        kak_assert(m_coord.column == 0);
        return true;
    }
    return offset() == m_buffer->character_count();
}

}
#endif // buffer_iterator_inl_h_INCLUDED
