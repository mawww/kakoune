#include "buffer.hh"

#include "buffer_manager.hh"
#include "window.hh"
#include "assert.hh"
#include "utils.hh"
#include "context.hh"

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

Buffer::Buffer(const String& name, Type type,
               const String& initial_content)
    : m_name(name), m_type(type),
      m_history(1), m_history_cursor(m_history.begin()),
      m_last_save_undo_index(0),
      m_hook_manager(GlobalHookManager::instance()),
      m_option_manager(GlobalOptionManager::instance())
{
    BufferManager::instance().register_buffer(this);
    if (not initial_content.empty())
        apply_modification(Modification::make_insert(begin(), initial_content));

    if (type == Type::NewFile)
        m_hook_manager.run_hook("BufNew", name, Context(*this));
    else if (type == Type::File)
        m_hook_manager.run_hook("BufOpen", name, Context(*this));

    m_hook_manager.run_hook("BufCreate", name, Context(*this));
}

Buffer::~Buffer()
{
    m_windows.clear();
    BufferManager::instance().unregister_buffer(this);
    assert(m_iterators_to_update.empty());
}

BufferIterator Buffer::iterator_at(const BufferCoord& line_and_column) const
{
    return BufferIterator(*this, clamp(line_and_column));
}

BufferCoord Buffer::line_and_column_at(const BufferIterator& iterator) const
{
    return iterator.m_coord;
}

BufferPos Buffer::line_at(const BufferIterator& iterator) const
{
    return iterator.line();
}

BufferSize Buffer::line_length(BufferPos line) const
{
    assert(line < line_count());
    BufferPos end = (line < m_lines.size() - 1) ?
                    m_lines[line + 1].start : length();
    return end - m_lines[line].start;
}

BufferCoord Buffer::clamp(const BufferCoord& line_and_column) const
{
    if (m_lines.empty())
        return BufferCoord();

    BufferCoord result(line_and_column.line, line_and_column.column);
    result.line = Kakoune::clamp<int>(0, m_lines.size() - 1, result.line);
    int max_col = std::max(0, line_length(result.line) - 2);
    result.column = Kakoune::clamp<int>(0, max_col, result.column);
    return result;
}

BufferIterator Buffer::iterator_at_line_begin(const BufferIterator& iterator) const
{
    return BufferIterator(*this, { iterator.line(), 0 });
}

BufferIterator Buffer::iterator_at_line_end(const BufferIterator& iterator) const
{
    BufferPos line = iterator.line();
    return ++BufferIterator(*this, { line, std::max(line_length(line) - 1, 0) });
}

BufferIterator Buffer::begin() const
{
    return BufferIterator(*this, { 0, 0 });
}

BufferIterator Buffer::end() const
{
    if (m_lines.empty())
        return BufferIterator(*this, { 0, 0 });
    return BufferIterator(*this, { (int)line_count()-1, (int)m_lines.back().length() });
}

BufferSize Buffer::length() const
{
    if (m_lines.empty())
        return 0;
    return m_lines.back().start + m_lines.back().length();
}

BufferSize Buffer::line_count() const
{
    return m_lines.size();
}

String Buffer::string(const BufferIterator& begin, const BufferIterator& end) const
{
    String res;
    for (BufferPos line = begin.line(); line <= end.line(); ++line)
    {
       size_t start = 0;
       if (line == begin.line())
           start = begin.column();
       size_t count = -1;
       if (line == end.line())
           count = end.column() - start;
       res += m_lines[line].content.substr(start, count);
    }
    return res;
}

void Buffer::begin_undo_group()
{
    assert(m_current_undo_group.empty());
    m_history.erase(m_history_cursor, m_history.end());

    if (m_history.size() < m_last_save_undo_index)
        m_last_save_undo_index = -1;

    m_history_cursor = m_history.end();
}

void Buffer::end_undo_group()
{
    if (m_current_undo_group.empty())
        return;

    m_history.push_back(m_current_undo_group);
    m_history_cursor = m_history.end();

    m_current_undo_group.clear();
}

Modification Modification::inverse() const
{
    Type inverse_type;
    switch (type)
    {
    case Insert: inverse_type = Erase;  break;
    case Erase:  inverse_type = Insert; break;
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
        apply_modification(modification.inverse());
    return true;
}

bool Buffer::redo()
{
    if (m_history_cursor == m_history.end())
        return false;

    for (const Modification& modification : *m_history_cursor)
        apply_modification(modification);

    ++m_history_cursor;
    return true;
}

void Buffer::check_invariant() const
{
    BufferSize start = 0;
    for (auto& line : m_lines)
    {
        assert(line.start == start);
        assert(line.length() > 0);
        start += line.length();
    }
}

void Buffer::insert(const BufferIterator& pos, const String& content)
{
    BufferSize offset = pos.offset();

    // all following lines advanced by length
    for (size_t i = pos.line()+1; i < line_count(); ++i)
        m_lines[i].start += content.length();

    BufferCoord end_pos;
    // if we inserted at the end of the buffer, we may have created a new
    // line without inserting a '\n'
    if (pos == end() and (pos == begin() or *(pos-1) == '\n'))
    {
        int start = 0;
        for (int i = 0; i < content.length(); ++i)
        {
            if (content[i] == '\n')
            {
                m_lines.push_back({ offset + start, content.substr(start, i + 1 - start) });
                start = i + 1;
            }
        }
        if (start != content.length())
            m_lines.push_back({ offset + start, content.substr(start) });

        end_pos = end().m_coord;
    }
    else
    {
        String prefix = m_lines[pos.line()].content.substr(0, pos.column());
        String suffix = m_lines[pos.line()].content.substr(pos.column());

        auto line_it = m_lines.begin() + pos.line();
        line_it = m_lines.erase(line_it);

        int start = 0;
        for (int i = 0; i < content.length(); ++i)
        {
            if (content[i] == '\n')
            {
                String line_content = content.substr(start, i + 1 - start);
                if (start == 0)
                {
                    line_content = prefix + line_content;
                    line_it = m_lines.insert(line_it, { offset + start - (int)prefix.length(),
                                                        std::move(line_content) });
                }
                else
                    line_it = m_lines.insert(line_it, { offset + start,
                                                        std::move(line_content) });

                ++line_it;
                start = i + 1;
            }
        }
        if (start == 0)
            line_it = m_lines.insert(line_it, { offset + start - (int)prefix.length(), prefix + content + suffix });
        else
            line_it = m_lines.insert(line_it, { offset + start, content.substr(start) + suffix });

        end_pos = { int(line_it - m_lines.begin()), int(line_it->length() - suffix.length()) };
    }

    check_invariant();

    for (auto iterator : m_iterators_to_update)
        iterator->on_insert(pos.m_coord, end_pos);
}

void Buffer::erase(const BufferIterator& pos, BufferSize length)
{
    BufferIterator end = pos + length;
    String prefix = m_lines[pos.line()].content.substr(0, pos.column());
    String suffix = m_lines[end.line()].content.substr(end.column());
    Line new_line = { m_lines[pos.line()].start, prefix + suffix };

    m_lines.erase(m_lines.begin() + pos.line(), m_lines.begin() + end.line() + 1);
    if (new_line.length())
        m_lines.insert(m_lines.begin() + pos.line(), std::move(new_line));

    for (size_t i = pos.line()+1; i < line_count(); ++i)
        m_lines[i].start -= length;

    check_invariant();

    for (auto iterator : m_iterators_to_update)
        iterator->on_erase(pos.m_coord, end.m_coord);
}

void Buffer::apply_modification(const Modification& modification)
{
    const String& content = modification.content;
    const BufferIterator& pos = modification.position;

    switch (modification.type)
    {
    case Modification::Insert:
    {
        BufferIterator pos = modification.position < end() ?
                             modification.position : end();
        insert(pos, modification.content);
        break;
    }
    case Modification::Erase:
    {
        size_t count = modification.content.length();
        assert(string(modification.position, modification.position + count)
               == modification.content);
        erase(modification.position, count);
        break;
    }
    default:
        assert(false);
    }
}

void Buffer::modify(Modification&& modification)
{
    if (modification.content.empty())
        return;

    apply_modification(modification);
    m_current_undo_group.push_back(std::move(modification));
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

bool Buffer::is_modified() const
{
    size_t history_cursor_index = m_history_cursor - m_history.begin();
    return m_last_save_undo_index != history_cursor_index
           or not m_current_undo_group.empty();
}

void Buffer::notify_saved()
{
    size_t history_cursor_index = m_history_cursor - m_history.begin();
    m_last_save_undo_index = history_cursor_index;
}

void Buffer::add_iterator_to_update(BufferIterator& iterator)
{
    assert(not contains(m_iterators_to_update, &iterator));
    m_iterators_to_update.push_back(&iterator);
}

void Buffer::remove_iterator_from_update(BufferIterator& iterator)
{
    auto it = std::find(m_iterators_to_update.begin(),
                        m_iterators_to_update.end(),
                        &iterator);
    assert(it != m_iterators_to_update.end());
    m_iterators_to_update.erase(it);
}

}
