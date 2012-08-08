#ifndef buffer_iterator_inl_h_INCLUDED
#define buffer_iterator_inl_h_INCLUDED

#include "assert.hh"

namespace Kakoune
{

inline BufferIterator::BufferIterator(const Buffer& buffer, BufferCoord coord)
    : m_buffer(&buffer), m_coord(coord)
{
    assert(is_valid());
}

inline const Buffer& BufferIterator::buffer() const
{
    assert(m_buffer);
    return *m_buffer;
}

inline bool BufferIterator::is_valid() const
{
    return m_buffer and
           ((line() < m_buffer->line_count() and
             column() < m_buffer->m_lines[line()].length()) or
            ((line() == m_buffer->line_count() and column() == 0)) or
             (line() == m_buffer->line_count() - 1 and
              column() == m_buffer->m_lines.back().length()));
}

inline bool BufferIterator::operator==(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_coord == iterator.m_coord);
}

inline bool BufferIterator::operator!=(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_coord != iterator.m_coord);
}

inline bool BufferIterator::operator<(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_coord < iterator.m_coord);
}

inline bool BufferIterator::operator<=(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_coord <= iterator.m_coord);
}

inline bool BufferIterator::operator>(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_coord > iterator.m_coord);
}

inline bool BufferIterator::operator>=(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_coord >= iterator.m_coord);
}

inline void BufferIterator::on_insert(const BufferCoord& begin,
                                      const BufferCoord& end)
{
    if (m_coord < begin)
        return;

    if (begin.line == line())
        m_coord.column = end.column + m_coord.column - begin.column;
    m_coord.line += end.line - begin.line;

    assert(is_valid());
}

inline void BufferIterator::on_erase(const BufferCoord& begin,
                                     const BufferCoord& end)
{
    if (m_coord < begin)
        return;

    if (m_coord <= end)
        m_coord = begin;
    else
    {
        if (end.line == m_coord.line)
        {
            m_coord.line = begin.line;
            m_coord.column = begin.column + m_coord.column - end.column;
        }
        else
            m_coord.line -= end.line - begin.line;
    }

    if (is_end())
        operator--();
    assert(is_valid());
}


inline Character BufferIterator::operator*() const
{
    assert(m_buffer);
    return m_buffer->m_lines[line()].content[column()];
}

inline BufferSize BufferIterator::offset() const
{
    assert(m_buffer);
    return line() == 0 ? column() : m_buffer->m_lines[line()].start + column();
}

inline BufferSize BufferIterator::operator-(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return offset() - iterator.offset();
}

inline BufferIterator BufferIterator::operator+(BufferSize size) const
{
    assert(m_buffer);
    if (size >= 0)
    {
        BufferSize o = std::min(m_buffer->character_count(), offset() + size);
        for (int i = line() + 1; i < m_buffer->line_count(); ++i)
        {
            if (m_buffer->m_lines[i].start > o)
                return BufferIterator(*m_buffer, { i-1, o - m_buffer->m_lines[i-1].start });
        }
        int last_line = std::max(0, m_buffer->line_count() - 1);
        return BufferIterator(*m_buffer, { last_line, o - m_buffer->m_lines[last_line].start });
    }
    return operator-(-size);
}

inline BufferIterator BufferIterator::operator-(BufferSize size) const
{
    assert(m_buffer);
    if (size >= 0)
    {
        BufferSize o = std::max(0, offset() - size);
        for (int i = line(); i >= 0; --i)
        {
            if (m_buffer->m_lines[i].start <= o)
                return BufferIterator(*m_buffer, { i, o - m_buffer->m_lines[i].start });
        }
        assert(false);
    }
    return operator+(-size);
}

inline BufferIterator& BufferIterator::operator+=(BufferSize size)
{
    return *this = (*this + size);
}

inline BufferIterator& BufferIterator::operator-=(BufferSize size)
{
    return *this = (*this - size);
}

inline BufferIterator& BufferIterator::operator++()
{
    if (column() < m_buffer->m_lines[line()].length() - 1)
        ++m_coord.column;
    else if (line() == m_buffer->line_count() - 1)
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

inline bool BufferIterator::is_begin() const
{
    assert(m_buffer);
    return m_coord.line == 0 and m_coord.column == 0;
}

inline bool BufferIterator::is_end() const
{
    assert(m_buffer);
    return offset() == m_buffer->character_count();
}

}
#endif // buffer_iterator_inl_h_INCLUDED
