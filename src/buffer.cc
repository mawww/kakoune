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

Buffer::Buffer(String name, Flags flags, std::vector<String> lines)
    : m_name(std::move(name)), m_flags(flags | Flags::NoUndo),
      m_history(), m_history_cursor(m_history.begin()),
      m_last_save_undo_index(0),
      m_timestamp(0),
      m_hooks(GlobalHooks::instance()),
      m_options(GlobalOptions::instance())
{
    BufferManager::instance().register_buffer(*this);

    if (lines.empty())
        lines.emplace_back("\n");

    ByteCount pos = 0;
    m_lines.reserve(lines.size());
    for (auto& line : lines)
    {
        assert(not line.empty() and line.back() == '\n');
        m_lines.emplace_back(Line{ pos, std::move(line) });
        pos += m_lines.back().length();
    }

    Editor editor_for_hooks(*this);
    Context context(editor_for_hooks);
    if (flags & Flags::File and flags & Flags::New)
        m_hooks.run_hook("BufNew", m_name, context);
    else
        m_hooks.run_hook("BufOpen", m_name, context);

    m_hooks.run_hook("BufCreate", m_name, context);

    // now we may begin to record undo data
    m_flags = flags;
}

Buffer::~Buffer()
{
    {
        Editor hook_editor{*this};
        Context hook_context{hook_editor};
        m_hooks.run_hook("BufClose", m_name, hook_context);
    }

    BufferManager::instance().unregister_buffer(*this);
    assert(m_change_listeners.empty());
}

BufferIterator Buffer::iterator_at(const BufferCoord& line_and_column,
                                   bool avoid_eol) const
{
    return BufferIterator(*this, clamp(line_and_column, avoid_eol));
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
    line = Kakoune::clamp(line, 0_line, line_count()-1);
    assert(line_length(line) > 0);
    return BufferIterator(*this, { line, 0 });
}

BufferIterator Buffer::iterator_at_line_end(const BufferIterator& iterator) const
{
    LineCount line = iterator.line();
    assert(line_length(line) > 0);
    return ++BufferIterator(*this, { line, line_length(line) - 1 });
}

BufferIterator Buffer::iterator_at_line_end(LineCount line) const
{
    line = Kakoune::clamp(line, 0_line, line_count()-1);
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
    if (m_flags & Flags::NoUndo)
        return;

    assert(m_current_undo_group.empty());
    m_history.erase(m_history_cursor, m_history.end());

    if (m_history.size() < m_last_save_undo_index)
        m_last_save_undo_index = -1;

    m_history_cursor = m_history.end();
}

void Buffer::end_undo_group()
{
    if (m_flags & Flags::NoUndo)
        return;

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
        return {inverse_type, position, content};
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

void Buffer::check_invariant() const
{
    ByteCount start = 0;
    assert(not m_lines.empty());
    for (auto& line : m_lines)
    {
        assert(line.start == start);
        assert(line.length() > 0);
        assert(line.content.back() == '\n');
        assert(find(line.content, '\n') == line.content.end()-1);
        start += line.length();
    }
}

void Buffer::do_insert(const BufferIterator& pos, const String& content)
{
    assert(pos.is_valid() and (pos.is_end() or utf8::is_character_start(pos)));
    assert(not contains(content, '\0'));
    ++m_timestamp;
    ByteCount offset = pos.offset();

    // all following lines advanced by length
    for (LineCount i = pos.line()+1; i < line_count(); ++i)
        m_lines[i].start += content.length();

    BufferIterator begin_it;
    BufferIterator end_it;
    // if we inserted at the end of the buffer, we have created a new
    // line without inserting a '\n'
    if (pos.is_end())
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

        begin_it = BufferIterator{*this, { pos.line() + 1, 0 }};
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
    assert(begin.is_valid());
    assert(end.is_valid());
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
    BufferIterator pos = modification.position;

    // this may happen when a modification applied at the
    // end of the buffer has been inverted for an undo.
    if (pos.column() == m_lines[pos.line()].length())
        pos = { pos.buffer(), { pos.line() + 1, 0 }};

    assert(pos.is_valid());
    switch (modification.type)
    {
    case Modification::Insert:
    {
        do_insert(pos, content);
        break;
    }
    case Modification::Erase:
    {
        ByteCount count = content.length();
        BufferIterator end = pos + count;
        assert(string(pos, end) == content);
        do_erase(pos, end);
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

    if (not (m_flags & Flags::NoUndo))
        m_current_undo_group.emplace_back(Modification::Insert, pos, content);
    do_insert(pos, content);
}

void Buffer::erase(BufferIterator begin, BufferIterator end)
{
    if (end.is_end() and (begin.column() != 0 or begin.is_begin()))
        --end;

    if (begin == end)
        return;

    if (not (m_flags & Flags::NoUndo))
        m_current_undo_group.emplace_back(Modification::Erase, begin,
                                          string(begin, end));
    do_erase(begin, end);
}

bool Buffer::is_modified() const
{
    size_t history_cursor_index = m_history_cursor - m_history.begin();
    return m_last_save_undo_index != history_cursor_index
           or not m_current_undo_group.empty();
}

void Buffer::notify_saved()
{
    if (not m_current_undo_group.empty())
    {
        end_undo_group();
        begin_undo_group();
    }

    m_flags &= ~Flags::New;
    size_t history_cursor_index = m_history_cursor - m_history.begin();
    if (m_last_save_undo_index != history_cursor_index)
    {
        ++m_timestamp;
        m_last_save_undo_index = history_cursor_index;
    }
}

}
