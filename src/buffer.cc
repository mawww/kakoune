#include "buffer.hh"
#include <cassert>

namespace Kakoune
{

template<typename T>
T clamp(T min, T max, T val)
{
    if (val < min)
        return min;
    if (val > max)
        return max;
    return val;
}

BufferIterator::BufferIterator(const Buffer& buffer, BufferPos position) : m_buffer(&buffer),
      m_position(std::max(0, std::min(position, (BufferPos)buffer.length())))
{
}

const Buffer& BufferIterator::buffer() const
{
    assert(m_buffer);
    return *m_buffer;
}

BufferIterator& BufferIterator::operator=(const BufferIterator& iterator)
{
    m_buffer == iterator.m_buffer;
    m_position = iterator.m_position;
}

bool BufferIterator::operator==(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_position == iterator.m_position);
}

bool BufferIterator::operator!=(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_position != iterator.m_position);
}

bool BufferIterator::operator<(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_position < iterator.m_position);
}

bool BufferIterator::operator<=(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_position <= iterator.m_position);
}

BufferChar BufferIterator::operator*() const
{
    assert(m_buffer);
    return m_buffer->at(m_position);
}

BufferSize BufferIterator::operator-(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return static_cast<BufferSize>(m_position) -
           static_cast<BufferSize>(iterator.m_position);
}

BufferIterator BufferIterator::operator+(BufferSize size) const
{
    assert(m_buffer);
    return BufferIterator(*m_buffer, m_position + size);
}

BufferIterator BufferIterator::operator-(BufferSize size) const
{
    assert(m_buffer);
    return BufferIterator(*m_buffer, m_position - size);
}

BufferIterator& BufferIterator::operator+=(BufferSize size)
{
    assert(m_buffer);
    m_position = std::max(0, std::min((BufferSize)m_position + size,
                                      m_buffer->length()));
    return *this;
}

BufferIterator& BufferIterator::operator-=(BufferSize size)
{
    assert(m_buffer);
    m_position = std::max(0, std::min((BufferSize)m_position - size,
                                      m_buffer->length()));
    return *this;
}

BufferIterator& BufferIterator::operator++()
{
    return (*this += 1);
}

BufferIterator& BufferIterator::operator--()
{
    return (*this -= 1);
}

bool BufferIterator::is_begin() const
{
    assert(m_buffer);
    return m_position == 0;
}

bool BufferIterator::is_end() const
{
    assert(m_buffer);
    return m_position == m_buffer->length();
}

Buffer::Buffer(const std::string& name)
    : m_name(name)
{
}

void Buffer::erase(const BufferIterator& begin, const BufferIterator& end)
{
    m_content.erase(begin.m_position, end - begin);
    compute_lines();
}

void Buffer::insert(const BufferIterator& position, const BufferString& string)
{
    m_content.insert(position.m_position, string);
    compute_lines();
}

BufferIterator Buffer::iterator_at(const BufferCoord& line_and_column) const
{
    if (m_lines.empty())
        return begin();

    BufferPos line   = Kakoune::clamp<int>(0, m_lines.size() - 1, line_and_column.line);
    BufferPos column = Kakoune::clamp<int>(0, line_length(line),  line_and_column.column);
    return BufferIterator(*this, m_lines[line] + column);
}

BufferCoord Buffer::line_and_column_at(const BufferIterator& iterator) const
{
    BufferCoord result;
    if (not m_lines.empty())
    {
        result.line = line_at(iterator);
        result.column = iterator.m_position - m_lines[result.line];
    }
    return result;
}

BufferPos Buffer::line_at(const BufferIterator& iterator) const
{
    for (unsigned i = 0; i < m_lines.size(); ++i)
    {
        if (m_lines[i] > iterator.m_position)
            return i - 1;
    }
    return m_lines.size() - 1;
}

BufferSize Buffer::line_length(BufferPos line) const
{
    assert(not m_lines.empty());
    BufferPos end = (line >= m_lines.size() - 1) ?
                    m_content.size() : m_lines[line + 1] - 1;
    return end - m_lines[line];
}

BufferCoord Buffer::clamp(const BufferCoord& line_and_column) const
{
    if (m_lines.empty())
        return BufferCoord();

    BufferCoord result(line_and_column.line, line_and_column.column);
    result.line = Kakoune::clamp<int>(0, m_lines.size() - 1, result.line);
    result.column = Kakoune::clamp<int>(0, line_length(result.line), result.column);
    return result;
}

void Buffer::compute_lines()
{
    m_lines.clear();
    m_lines.push_back(0);
    for (BufferPos i = 0; i < m_content.size(); ++i)
    {
        if (m_content[i] == '\n')
            m_lines.push_back(i + 1);
    }
}

BufferIterator Buffer::begin() const
{
    return BufferIterator(*this, 0);
}

BufferIterator Buffer::end() const
{
    return BufferIterator(*this, length());
}

BufferSize Buffer::length() const
{
    return m_content.size();
}

BufferString Buffer::string(const BufferIterator& begin, const BufferIterator& end) const
{
    return m_content.substr(begin.m_position, end - begin);
}

BufferChar Buffer::at(BufferPos position) const
{
    return m_content[position];
}

}
