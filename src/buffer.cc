#include "buffer.hh"

#include "buffer_manager.hh"
#include "window.hh"
#include "assert.hh"
#include "utils.hh"
#include "context.hh"
#include "utf8.hh"

#include <algorithm>

namespace Kakoune
{

Buffer::Buffer(String name, Type type,
               String initial_content)
    : m_name(std::move(name)), m_type(type),
      m_history(1), m_history_cursor(m_history.begin()),
      m_last_save_undo_index(0),
      m_timestamp(0),
      m_hook_manager(GlobalHookManager::instance()),
      m_option_manager(GlobalOptionManager::instance())
{
    BufferManager::instance().register_buffer(*this);
    if (initial_content.back() != '\n')
        initial_content += '\n';
    do_insert(begin(), std::move(initial_content));


    Editor editor_for_hooks(*this);
    Context context(editor_for_hooks);
    if (type == Type::NewFile)
        m_hook_manager.run_hook("BufNew", m_name, context);
    else if (type == Type::File)
        m_hook_manager.run_hook("BufOpen", m_name, context);

    m_hook_manager.run_hook("BufCreate", m_name, context);

    reset_undo_data();
}

Buffer::~Buffer()
{
    m_hook_manager.run_hook("BufClose", m_name, Context(Editor(*this)));

    m_windows.clear();
    BufferManager::instance().unregister_buffer(*this);
    assert(m_change_listeners.empty());
}

BufferIterator Buffer::iterator_at(const BufferCoord& line_and_column,
                                   bool avoid_eol) const
{
    return BufferIterator(*this, clamp(line_and_column, avoid_eol));
}

BufferCoord Buffer::line_and_column_at(const BufferIterator& iterator) const
{
    return iterator.m_coord;
}

ByteCount Buffer::line_length(LineCount line) const
{
    assert(line < line_count());
    ByteCount end = (line < line_count() - 1) ?
                    m_lines[line + 1].start : character_count();
    return end - m_lines[line].start;
}

BufferCoord Buffer::clamp(const BufferCoord& line_and_column,
                          bool avoid_eol) const
{
    if (m_lines.empty())
        return BufferCoord();

    BufferCoord result(line_and_column.line, line_and_column.column);
    result.line = Kakoune::clamp(result.line, 0_line, line_count() - 1);
    ByteCount max_col = std::max(0_byte, line_length(result.line) - (avoid_eol ? 2 : 1));
    result.column = Kakoune::clamp(result.column, 0_byte, max_col);
    return result;
}

BufferIterator Buffer::iterator_at_line_begin(const BufferIterator& iterator) const
{
    return BufferIterator(*this, { iterator.line(), 0 });
}

BufferIterator Buffer::iterator_at_line_begin(LineCount line) const
{
    return BufferIterator(*this, clamp({ line, 0 }));
}

BufferIterator Buffer::iterator_at_line_end(const BufferIterator& iterator) const
{
    LineCount line = iterator.line();
    assert(line_length(line) > 0);
    return ++BufferIterator(*this, { line, line_length(line) - 1 });
}

BufferIterator Buffer::iterator_at_line_end(LineCount line) const
{
    line = std::min(line, line_count()-1);
    assert(line_length(line) > 0);
    return ++BufferIterator(*this, { line, line_length(line) - 1 });
}

BufferIterator Buffer::begin() const
{
    return BufferIterator(*this, { 0_line, 0 });
}

BufferIterator Buffer::end() const
{
    if (m_lines.empty())
        return BufferIterator(*this, { 0_line, 0 });
    return BufferIterator(*this, { line_count()-1, m_lines.back().length() });
}

ByteCount Buffer::character_count() const
{
    if (m_lines.empty())
        return 0;
    return m_lines.back().start + m_lines.back().length();
}

LineCount Buffer::line_count() const
{
    return LineCount(m_lines.size());
}

String Buffer::string(const BufferIterator& begin, const BufferIterator& end) const
{
    String res;
    for (LineCount line = begin.line(); line <= end.line(); ++line)
    {
       ByteCount start = 0;
       if (line == begin.line())
           start = begin.column();
       ByteCount count = -1;
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

    m_history.push_back(std::move(m_current_undo_group));
    m_history_cursor = m_history.end();

    m_current_undo_group.clear();
}

// A Modification holds a single atomic modification to Buffer
struct Buffer::Modification
{
    enum Type { Insert, Erase };

    Type           type;
    BufferIterator position;
    String         content;

    Modification(Type type, BufferIterator position, String content)
        : type(type), position(position), content(std::move(content)) {}

    Modification inverse() const
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
};

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

void Buffer::reset_undo_data()
{
   m_history.clear();
   m_history_cursor = m_history.end();
   m_current_undo_group.clear();
}

void Buffer::check_invariant() const
{
    ByteCount start = 0;
    assert(not m_lines.empty());
    for (auto& line : m_lines)
    {
        assert(line.start == start);
        assert(line.length() > 0);
        assert(line.content.back() == '\n');
        start += line.length();
    }
}

void Buffer::do_insert(const BufferIterator& pos, const String& content)
{
    assert(pos.is_end() or utf8::is_character_start(pos));
    ++m_timestamp;
    ByteCount offset = pos.offset();

    // all following lines advanced by length
    for (LineCount i = pos.line()+1; i < line_count(); ++i)
        m_lines[i].start += content.length();

    BufferIterator begin_it;
    BufferIterator end_it;
    // if we inserted at the end of the buffer, we may have created a new
    // line without inserting a '\n'
    if (pos == end() and (pos == begin() or *(pos-1) == '\n'))
    {
        ByteCount start = 0;
        for (ByteCount i = 0; i < content.length(); ++i)
        {
            if (content[i] == '\n')
            {
                m_lines.push_back({ offset + start, content.substr(start, i + 1 - start) });
                start = i + 1;
            }
        }
        if (start != content.length())
            m_lines.push_back({ offset + start, content.substr(start) });

        begin_it = pos;
        end_it = end();
    }
    else
    {
        String prefix = m_lines[pos.line()].content.substr(0, pos.column());
        String suffix = m_lines[pos.line()].content.substr(pos.column());

        auto line_it = m_lines.begin() + (int)pos.line();
        line_it = m_lines.erase(line_it);

        ByteCount start = 0;
        for (ByteCount i = 0; i < content.length(); ++i)
        {
            if (content[i] == '\n')
            {
                String line_content = content.substr(start, i + 1 - start);
                if (start == 0)
                {
                    line_content = prefix + line_content;
                    line_it = m_lines.insert(line_it, { offset + start - prefix.length(),
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
            line_it = m_lines.insert(line_it, { offset + start - prefix.length(), prefix + content + suffix });
        else if (start != content.length() or not suffix.empty())
            line_it = m_lines.insert(line_it, { offset + start, content.substr(start) + suffix });
        else
            --line_it;

        begin_it = pos;
        end_it = BufferIterator(*this, { LineCount(line_it - m_lines.begin()),
                                         line_it->length() - suffix.length() });
    }

    check_invariant();

    for (auto listener : m_change_listeners)
        listener->on_insert(begin_it, end_it);
}

void Buffer::do_erase(const BufferIterator& begin, const BufferIterator& end)
{
    assert(utf8::is_character_start(begin) and
           (end.is_end() or utf8::is_character_start(end)));
    ++m_timestamp;
    const ByteCount length = end - begin;
    String prefix = m_lines[begin.line()].content.substr(0, begin.column());
    String suffix = m_lines[end.line()].content.substr(end.column());
    Line new_line = { m_lines[begin.line()].start, prefix + suffix };

    m_lines.erase(m_lines.begin() + (int)begin.line(), m_lines.begin() + (int)end.line() + 1);
    if (new_line.length() != 0)
        m_lines.insert(m_lines.begin() + (int)begin.line(), std::move(new_line));

    for (LineCount i = begin.line()+1; i < line_count(); ++i)
        m_lines[i].start -= length;

    check_invariant();

    for (auto listener : m_change_listeners)
        listener->on_erase(begin, end);
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
        do_insert(pos, modification.content);
        break;
    }
    case Modification::Erase:
    {
        ByteCount count = modification.content.length();
        BufferIterator end = modification.position + count;
        assert(string(modification.position, end) == modification.content);
        do_erase(modification.position, end);
        break;
    }
    default:
        assert(false);
    }
}

void Buffer::insert(BufferIterator pos, String content)
{
    if (content.empty())
        return;

    if (pos.is_end() and content.back() != '\n')
        content += '\n';

    m_current_undo_group.emplace_back(Modification::Insert, pos,
                                      std::move(content));
    do_insert(pos, m_current_undo_group.back().content);
}

void Buffer::erase(BufferIterator begin, BufferIterator end)
{
    if (end.is_end() and (begin.column() != 0 or begin.is_begin()))
        --end;

    if (begin == end)
        return;

    m_current_undo_group.emplace_back(Modification::Erase, begin,
                                      string(begin, end));
    do_erase(begin, end);
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

void Buffer::add_change_listener(BufferChangeListener& listener)
{
    assert(not contains(m_change_listeners, &listener));
    m_change_listeners.push_back(&listener);
}

void Buffer::remove_change_listener(BufferChangeListener& listener)
{
    auto it = std::find(m_change_listeners.begin(),
                        m_change_listeners.end(),
                        &listener);
    assert(it != m_change_listeners.end());
    m_change_listeners.erase(it);
}

}
