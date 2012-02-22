#include "buffer.hh"

#include "buffer_manager.hh"
#include "window.hh"
#include "assert.hh"
#include "utils.hh"
#include "hooks_manager.hh"
#include "context.hh"

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

Buffer::Buffer(const std::string& name, Type type,
               const BufferString& initial_content)
    : m_name(name), m_type(type),
      m_history(1), m_history_cursor(m_history.begin()),
      m_content(initial_content), m_last_save_undo_index(0)
{
    BufferManager::instance().register_buffer(this);

    if (type == Type::NewFile)
        GlobalHooksManager::instance().run_hook("BufCreate", name, Context(*this));
    else if (type == Type::File)
        GlobalHooksManager::instance().run_hook("BufOpen", name, Context(*this));

    compute_lines();
}

Buffer::~Buffer()
{
    m_windows.clear();
    assert(m_modification_listeners.empty());
}

BufferIterator Buffer::iterator_at(const BufferCoord& line_and_column) const
{
    if (m_lines.empty())
        return begin();

    BufferCoord clamped = clamp(line_and_column);
    return BufferIterator(*this, m_lines[clamped.line] + clamped.column);
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
    for (unsigned i = 0; i < line_count(); ++i)
    {
        if (m_lines[i] > iterator.m_position)
            return i - 1;
    }
    return line_count() - 1;
}

BufferSize Buffer::line_length(BufferPos line) const
{
    assert(not m_lines.empty());
    BufferPos end = (line >= line_count() - 1) ?
                    m_content.size() : m_lines[line + 1] - 1;
    return end - m_lines[line];
}

BufferCoord Buffer::clamp(const BufferCoord& line_and_column) const
{
    if (m_lines.empty())
        return BufferCoord();

    BufferCoord result(line_and_column.line, line_and_column.column);
    result.line = Kakoune::clamp<int>(0, line_count() - 1, result.line);
    int max_col = std::max(0, line_length(result.line)-1);
    result.column = Kakoune::clamp<int>(0, max_col, result.column);
    return result;
}

BufferIterator Buffer::iterator_at_line_begin(const BufferIterator& iterator) const
{
    return BufferIterator(*this, m_lines[line_at(iterator)]);
}

BufferIterator Buffer::iterator_at_line_end(const BufferIterator& iterator) const
{
    BufferPos line = line_at(iterator) + 1;
    return line < m_lines.size() ? BufferIterator(*this, m_lines[line]) : end();
}

void Buffer::compute_lines()
{
    m_lines.clear();
    m_lines.push_back(0);
    for (BufferPos i = 0; i + 1 < m_content.size(); ++i)
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

    if (m_history.size() < m_last_save_undo_index)
        m_last_save_undo_index = -1;

    m_history_cursor = m_history.end();
}

void Buffer::end_undo_group()
{
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
}

bool Buffer::redo()
{
    if (m_history_cursor == m_history.end())
        return false;

    for (const Modification& modification : *m_history_cursor)
        apply_modification(modification);

    ++m_history_cursor;
}

void Buffer::apply_modification(const Modification& modification)
{
    switch (modification.type)
    {
    case Modification::Insert:
        m_content.insert(modification.position.m_position,
                         modification.content);
        break;
    case Modification::Erase:
    {
        size_t size = modification.content.size();
        assert(string(modification.position, modification.position + size)
               == modification.content);
        m_content.erase(modification.position.m_position, size);
        break;
    }
    default:
        assert(false);
    }
    compute_lines();
    for (auto listener : m_modification_listeners)
        listener->on_modification(modification);
}

void Buffer::modify(Modification&& modification)
{
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

void Buffer::register_modification_listener(ModificationListener* listener)
{
    assert(listener);
    assert(not contains(m_modification_listeners, listener));
    m_modification_listeners.push_back(listener);
}

void Buffer::unregister_modification_listener(ModificationListener* listener)
{
    assert(listener);
    auto it = std::find(m_modification_listeners.begin(),
                        m_modification_listeners.end(),
                        listener);
    assert(it != m_modification_listeners.end());
    m_modification_listeners.erase(it);
}

}
