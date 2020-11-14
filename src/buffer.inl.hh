#ifndef buffer_inl_h_INCLUDED
#define buffer_inl_h_INCLUDED

#include "assert.hh"

namespace Kakoune
{

[[gnu::always_inline]]
inline const char& Buffer::byte_at(BufferCoord c) const
{
    kak_assert(c.line < line_count() and c.column < m_lines[c.line].length());
    return m_lines[c.line][c.column];
}

inline BufferCoord Buffer::next(BufferCoord coord) const
{
    if (coord.column < m_lines[coord.line].length() - 1)
        return {coord.line, coord.column + 1};
    return { coord.line + 1, 0 };
}

inline BufferCoord Buffer::prev(BufferCoord coord) const
{
    if (coord.column == 0)
        return { coord.line - 1, m_lines[coord.line - 1].length() - 1 };
    return { coord.line, coord.column - 1 };
}

inline ByteCount Buffer::distance(BufferCoord begin, BufferCoord end) const
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

inline bool Buffer::is_valid(BufferCoord c) const
{
    return (c.line >= 0 and c.column >= 0) and
           ((c.line < line_count() and c.column < m_lines[c.line].length()) or
            (c.line == line_count() and c.column == 0));
}

inline bool Buffer::is_end(BufferCoord c) const
{
    return c >= end_coord();
}

inline BufferIterator Buffer::begin() const
{
    return {*this, { 0, 0 }};
}

inline BufferIterator Buffer::end() const
{
    return {*this, end_coord()};
}

[[gnu::always_inline]]
inline LineCount Buffer::line_count() const
{
    return LineCount{(int)m_lines.size()};
}

inline size_t Buffer::timestamp() const
{
    return m_changes.size();
}

inline StringView Buffer::substr(BufferCoord begin, BufferCoord end) const
{
    kak_assert(begin.line == end.line);
    return m_lines[begin.line].substr(begin.column, end.column - begin.column);
}

inline ConstArrayView<Buffer::Change> Buffer::changes_since(size_t timestamp) const
{
    if (timestamp < m_changes.size())
        return { m_changes.data() + timestamp,
                 m_changes.data() + m_changes.size() };
    return {};
}

inline BufferCoord Buffer::back_coord() const
{
    return { line_count() - 1, m_lines.back().length() - 1 };
}

inline BufferCoord Buffer::end_coord() const
{
    return line_count();
}

inline BufferIterator::BufferIterator(const Buffer& buffer, BufferCoord coord) noexcept
    : m_buffer{&buffer}, m_coord{coord},
      m_line{coord.line < buffer.line_count() ? (*m_buffer)[coord.line] : StringView{}} {}

inline bool BufferIterator::operator==(const BufferIterator& iterator) const noexcept
{
    return m_buffer == iterator.m_buffer and m_coord == iterator.m_coord;
}

inline std::strong_ordering BufferIterator::operator<=>(const BufferIterator& iterator) const noexcept
{
    kak_assert(m_buffer == iterator.m_buffer);
    return (m_coord <=> iterator.m_coord);
}

inline bool BufferIterator::operator==(const BufferCoord& coord) const noexcept
{
    return m_coord == coord;
}

[[gnu::always_inline]]
inline const char& BufferIterator::operator*() const noexcept
{
    return m_line[m_coord.column];
}

inline const char& BufferIterator::operator[](size_t n) const noexcept
{
    return m_buffer->byte_at(m_buffer->advance(m_coord, n));
}

inline size_t BufferIterator::operator-(const BufferIterator& iterator) const
{
    kak_assert(m_buffer == iterator.m_buffer);
    return (size_t)m_buffer->distance(iterator.m_coord, m_coord);
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
    m_line = (*m_buffer)[m_coord.line];
    return *this;
}

inline BufferIterator& BufferIterator::operator-=(ByteCount size)
{
    m_coord = m_buffer->advance(m_coord, -size);
    m_line = (*m_buffer)[m_coord.line];
    return *this;
}

inline BufferIterator& BufferIterator::operator++()
{
    if (++m_coord.column == m_line.length())
    {
        m_line = (++m_coord.line < m_buffer->line_count()) ?
            (*m_buffer)[m_coord.line] : StringView{};
        m_coord.column = 0;
    }
    return *this;
}

inline BufferIterator& BufferIterator::operator--()
{
    if (m_coord.column == 0)
    {
        m_line = (*m_buffer)[--m_coord.line];
        m_coord.column = m_line.length() - 1;
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
