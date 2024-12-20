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

inline ByteCount Buffer::distance(ArrayView<const StringDataPtr> lines, BufferCoord begin, BufferCoord end)
{
    if (begin > end)
        return -distance(lines, end, begin);
    if (begin.line == end.line)
        return end.column - begin.column;

    ByteCount res = lines[(size_t)begin.line]->length - begin.column;
    for (LineCount l = begin.line+1; l < end.line; ++l)
        res += lines[(size_t)l]->length;
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
    return {m_sentinel.get(), end_coord()};
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
    : BufferIterator{buffer.line_storage(coord.line).get(), coord} {}

inline BufferIterator::BufferIterator(const StringData* line, BufferCoord coord) noexcept
    : m_line{line},
      m_coord{coord} {}

inline bool BufferIterator::operator==(const BufferIterator& iterator) const noexcept
{
    return m_coord == iterator.m_coord;
}

inline auto BufferIterator::operator<=>(const BufferIterator& iterator) const noexcept
{
    return (m_coord <=> iterator.m_coord);
}

inline bool BufferIterator::operator==(const BufferCoord& coord) const noexcept
{
    return m_coord == coord;
}

[[gnu::always_inline]]
inline const char& BufferIterator::operator*() const noexcept
{
    return m_line->data()[(size_t)(int)m_coord.column];
}

inline BufferIterator advance(BufferIterator iterator, int n)
{
    for (; n > 0; --n)
        ++iterator;
    for (; n < 0; ++n)
        --iterator;
    return iterator;
}

inline const char& BufferIterator::operator[](size_t n) const noexcept
{
    return *advance(*this, n);
}

inline size_t BufferIterator::operator-(const BufferIterator& iterator) const
{
    size_t distance = 0;
    auto* line = iterator.m_line;
    while (line != m_line)
    {
        distance += line->length - (line == iterator.m_line ? (int)iterator.m_coord.column : 0);
    }
    distance += (int)(m_coord.column - (line == iterator.m_line ? (int)iterator.m_coord.column : 0));
    return distance;
}

inline BufferIterator BufferIterator::operator+(ByteCount size) const
{
    kak_assert(*this);
    return advance(*this, (int)size);
}

inline BufferIterator BufferIterator::operator-(ByteCount size) const
{
    return advance(*this, -(int)size);
}

inline BufferIterator& BufferIterator::operator+=(ByteCount size)
{
    return *this = advance(*this, (int)size);
}

inline BufferIterator& BufferIterator::operator-=(ByteCount size)
{
    return *this = advance(*this, -(int)size);
}

inline BufferIterator& BufferIterator::operator++()
{
    if (++m_coord.column == m_line->length)
    {
        m_line = m_line->next;
        ++m_coord.line;
        m_coord.column = 0;
    }
    return *this;
}

inline BufferIterator& BufferIterator::operator--()
{
    if (m_coord.column == 0)
    {
        m_line = m_line->prev;
        --m_coord.line;
        m_coord.column = m_line->length - 1;
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
