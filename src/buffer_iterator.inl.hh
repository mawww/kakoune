#ifndef buffer_iterator_inl_h_INCLUDED
#define buffer_iterator_inl_h_INCLUDED

#include "assert.hh"

namespace Kakoune
{

inline BufferIterator::BufferIterator(const Buffer& buffer, BufferPos position)
    : m_buffer(&buffer),
      m_position(std::max(0, std::min(position, (BufferPos)buffer.length())))
{
}

inline const Buffer& BufferIterator::buffer() const
{
    assert(m_buffer);
    return *m_buffer;
}

inline bool BufferIterator::is_valid() const
{
    return m_buffer;
}

inline BufferIterator& BufferIterator::operator=(const BufferIterator& iterator)
{
    m_buffer = iterator.m_buffer;
    m_position = iterator.m_position;
    return *this;
}

inline bool BufferIterator::operator==(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_position == iterator.m_position);
}

inline bool BufferIterator::operator!=(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_position != iterator.m_position);
}

inline bool BufferIterator::operator<(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_position < iterator.m_position);
}

inline bool BufferIterator::operator<=(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_position <= iterator.m_position);
}

inline bool BufferIterator::operator>(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_position > iterator.m_position);
}

inline bool BufferIterator::operator>=(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_position >= iterator.m_position);
}

inline BufferChar BufferIterator::operator*() const
{
    assert(m_buffer);
    return m_buffer->m_content[m_position];
}

inline BufferSize BufferIterator::operator-(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return m_position - iterator.m_position;
}

inline BufferIterator BufferIterator::operator+(BufferSize size) const
{
    assert(m_buffer);
    return BufferIterator(*m_buffer, m_position + size);
}

inline BufferIterator BufferIterator::operator-(BufferSize size) const
{
    assert(m_buffer);
    return BufferIterator(*m_buffer, m_position - size);
}

inline BufferIterator& BufferIterator::operator+=(BufferSize size)
{
    assert(m_buffer);
    m_position = std::max(0, std::min((BufferSize)m_position + size,
                                      m_buffer->length()));
    return *this;
}

inline BufferIterator& BufferIterator::operator-=(BufferSize size)
{
    assert(m_buffer);
    m_position = std::max(0, std::min((BufferSize)m_position - size,
                                      m_buffer->length()));
    return *this;
}

inline BufferIterator& BufferIterator::operator++()
{
    return (*this += 1);
}

inline BufferIterator& BufferIterator::operator--()
{
    return (*this -= 1);
}

inline bool BufferIterator::is_begin() const
{
    assert(m_buffer);
    return m_position == 0;
}

inline bool BufferIterator::is_end() const
{
    assert(m_buffer);
    return m_position == m_buffer->length();
}

}
#endif // buffer_iterator_inl_h_INCLUDED
