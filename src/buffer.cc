#include "buffer.hh"

#include "buffer_manager.hh"
#include "window.hh"
#include "assert.hh"
#include "utils.hh"

#include <algorithm>

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
    m_buffer = iterator.m_buffer;
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

bool BufferIterator::operator>(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_position > iterator.m_position);
}

bool BufferIterator::operator>=(const BufferIterator& iterator) const
{
    assert(m_buffer == iterator.m_buffer);
    return (m_position >= iterator.m_position);
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

Buffer::Buffer(const std::string& name, const BufferString& initial_content)
    : m_name(name), m_history(1), m_history_cursor(m_history.begin()),
      m_content(initial_content)
{
    BufferManager::instance().register_buffer(this);

    compute_lines();
}

void Buffer::erase(const BufferIterator& begin, const BufferIterator& end)
{
    append_modification(Modification(Modification::Erase, begin, string(begin, end)));
    do_erase(begin, end);
}

void Buffer::insert(const BufferIterator& position, const BufferString& string)
{
    append_modification(Modification(Modification::Insert, position, string));
    do_insert(position, string);
}

void Buffer::do_erase(const BufferIterator& begin, const BufferIterator& end)
{
    m_content.erase(begin.m_position, end - begin);
    compute_lines();
}

void Buffer::do_insert(const BufferIterator& position, const BufferString& string)
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

BufferSize Buffer::line_count() const
{
    return m_lines.size();
}

BufferString Buffer::string(const BufferIterator& begin, const BufferIterator& end) const
{
    return m_content.substr(begin.m_position, end - begin);
}

BufferChar Buffer::at(BufferPos position) const
{
    return m_content[position];
}

void Buffer::begin_undo_group()
{
    assert(m_current_undo_group.empty());
    m_history.erase(m_history_cursor, m_history.end());
    m_history_cursor = m_history.end();
}

void Buffer::end_undo_group()
{
    m_history.push_back(m_current_undo_group);
    m_history_cursor = m_history.end();

    m_current_undo_group.clear();
}

Buffer::Modification Buffer::Modification::inverse() const
{
    Modification::Type inverse_type;
    switch (type)
    {
    case Modification::Insert: inverse_type = Erase;  break;
    case Modification::Erase:  inverse_type = Insert; break;
    default: assert(false);
    }
    return Modification(inverse_type, position, content);
}

bool Buffer::undo()
{
    if (m_history_cursor == m_history.begin())
        return false;

    --m_history_cursor;

    for (const Modification& modification : reversed(*m_history_cursor))
        replay_modification(modification.inverse());
}

bool Buffer::redo()
{
    if (m_history_cursor == m_history.end())
        return false;

    for (const Modification& modification : *m_history_cursor)
        replay_modification(modification);

    ++m_history_cursor;
}

void Buffer::replay_modification(const Modification& modification)
{
    switch (modification.type)
    {
    case Modification::Insert:
        do_insert(modification.position, modification.content);
        break;
    case Modification::Erase:
    {
        BufferIterator begin = modification.position;
        BufferIterator end   = begin + modification.content.size();
        assert(string(begin, end) == modification.content);
        do_erase(begin, end);
        break;
    }
    default:
        assert(false);
    }
}

void Buffer::append_modification(Modification&& modification)
{
    m_current_undo_group.push_back(modification);
}

Window* Buffer::get_or_create_window()
{
    if (m_windows.empty())
        m_windows.push_front(std::unique_ptr<Window>(new Window(*this)));

    return m_windows.front().get();
}

void Buffer::delete_window(Window* window)
{
    assert(&window->buffer() == this);
    auto window_it = std::find(m_windows.begin(), m_windows.end(), window);
    assert(window_it != m_windows.end());
    m_windows.erase(window_it);
}

}
