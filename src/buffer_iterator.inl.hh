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

inline BufferIterator& BufferIterator::operator=(const BufferIterator& iterator)
{
    m_buffer = iterator.m_buffer;
    m_coord = iterator.m_coord;
    return *this;
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

inline BufferCoord measure_string(const String& string)
{
    BufferCoord res;
    for (auto c : string)
    {
        if (c == '\n')
        {
            ++res.line;
            res.column = 0;
        }
        else
           ++res.column;
    }
    return res;
}

inline BufferCoord advance(const BufferCoord& base, const BufferCoord& offset)
{
    if (offset.line == 0)
        return BufferCoord{base.line, base.column + offset.column};
    else
        return BufferCoord{base.line + offset.line, offset.column};
}

inline void BufferIterator::update(const Modification& modification)
{
    const BufferIterator& pos = modification.position;
    if (*this < pos)
        return;

    BufferCoord measure = measure_string(modification.content);
    if (modification.type == Modification::Erase)
    {
        BufferCoord end = advance(pos.m_coord, measure);
        if (m_coord <= end)
            m_coord = pos.m_coord;
        else
        {
            m_coord.line -= measure.line;
            if (end.line == m_coord.line)
                m_coord.column -= measure.column;
        }

        if (is_end())
            operator--();
    }
    else
    {
        assert(modification.type == Modification::Insert);
        if (pos.line() == line())
        {
            BufferCoord end = advance(pos.m_coord, measure);
            m_coord.column = end.column + column() - pos.column();
        }
        m_coord.line += measure.line;
    }
    assert(is_valid());
}

inline BufferChar BufferIterator::operator*() const
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
        BufferSize o = std::min(m_buffer->length(), offset() + size);
        for (int i = line() + 1; i < m_buffer->line_count(); ++i)
        {
            if (m_buffer->m_lines[i].start > o)
                return BufferIterator(*m_buffer, { i-1, o - m_buffer->m_lines[i-1].start });
        }
        int last_line = m_buffer->line_count() - 1;
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
}

inline BufferIterator& BufferIterator::operator--()
{
    return (*this -= 1);
}

inline bool BufferIterator::is_begin() const
{
    assert(m_buffer);
    return m_coord.line == 0 and m_coord.column == 0;
}

inline bool BufferIterator::is_end() const
{
    assert(m_buffer);
    return offset() == m_buffer->length();
}

}
#endif // buffer_iterator_inl_h_INCLUDED
