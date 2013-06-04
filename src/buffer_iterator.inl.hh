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

inline BufferIterator& BufferIterator::operator=(const BufferCoord& coord)
{
    kak_assert(m_buffer and m_buffer->is_valid(coord));
    m_coord = coord;
    return *this;
}

}
#endif // buffer_iterator_inl_h_INCLUDED
