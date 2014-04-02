#include "buffer.hh"

#include "assert.hh"
#include "buffer_manager.hh"
#include "context.hh"
#include "file.hh"
#include "utils.hh"
#include "window.hh"
#include "client.hh"

#include <algorithm>

namespace Kakoune
{

Buffer::Buffer(String name, Flags flags, std::vector<String> lines,
               time_t fs_timestamp)
    : m_name(flags & Flags::File ? real_path(parse_filename(name)) : std::move(name)),
      m_flags(flags | Flags::NoUndo),
      m_history(), m_history_cursor(m_history.begin()),
      m_last_save_undo_index(0),
      // start buffer timestamp at 1 so that caches can init to 0
      m_timestamp(1),
      m_fs_timestamp(fs_timestamp),
      m_hooks(GlobalHooks::instance()),
      m_options(GlobalOptions::instance()),
      m_keymaps(GlobalKeymaps::instance())
{
    BufferManager::instance().register_buffer(*this);
    m_options.register_watcher(*this);

    if (lines.empty())
        lines.emplace_back("\n");

    ByteCount pos = 0;
    m_lines.reserve(lines.size());
    for (auto& line : lines)
    {
        kak_assert(not line.empty() and line.back() == '\n');
        m_lines.emplace_back(Line{ m_timestamp, pos, std::move(line) });
        pos += m_lines.back().length();
    }

    if (flags & Flags::File)
    {
        if (flags & Flags::New)
            run_hook_in_own_context("BufNew", m_name);
        else
        {
            kak_assert(m_fs_timestamp != InvalidTime);
            run_hook_in_own_context("BufOpen", m_name);
        }
    }

    run_hook_in_own_context("BufCreate", m_name);

    // now we may begin to record undo data
    m_flags = flags;

    for (auto& option : m_options.flatten_options())
        on_option_changed(*option);
}

Buffer::~Buffer()
{
    run_hook_in_own_context("BufClose", m_name);

    m_options.unregister_watcher(*this);
    BufferManager::instance().unregister_buffer(*this);
    m_values.clear();
    kak_assert(m_change_listeners.empty());
}

void Buffer::reload(std::vector<String> lines, time_t fs_timestamp)
{
    // use back coord to simulate the persistance of the last end of line
    // as buffers are expected to never be empty.
    for (auto listener : m_change_listeners)
        listener->on_erase(*this, {0,0}, back_coord());

    m_history.clear();
    m_current_undo_group.clear();
    m_history_cursor = m_history.begin();
    m_last_save_undo_index = 0;
    m_lines.clear();
    ++m_timestamp;

    if (lines.empty())
        lines.emplace_back("\n");

    ByteCount pos = 0;
    m_lines.reserve(lines.size());
    for (auto& line : lines)
    {
        kak_assert(not line.empty() and line.back() == '\n');
        m_lines.emplace_back(Line{ m_timestamp, pos, std::move(line) });
        pos += m_lines.back().length();
    }
    m_fs_timestamp = fs_timestamp;

    for (auto listener : m_change_listeners)
        listener->on_insert(*this, {0,0}, back_coord());
}

String Buffer::display_name() const
{
    if (m_flags & Flags::File)
        return compact_path(m_name);
    return m_name;
}

bool Buffer::set_name(String name)
{
    Buffer* other = BufferManager::instance().get_buffer_ifp(name);
    if (other == nullptr or other == this)
    {
        if (m_flags & Flags::File)
            m_name = real_path(name);
        else
            m_name = std::move(name);
        return true;
    }
    return false;
}

BufferIterator Buffer::iterator_at(BufferCoord coord) const
{
    return is_end(coord) ? end() : BufferIterator(*this, clamp(coord));
}

BufferCoord Buffer::clamp(BufferCoord coord) const
{
    if (m_lines.empty())
        return BufferCoord{};

    coord.line = Kakoune::clamp(coord.line, 0_line, line_count() - 1);
    ByteCount max_col = std::max(0_byte, m_lines[coord.line].length() - 1);
    coord.column = Kakoune::clamp(coord.column, 0_byte, max_col);
    return coord;
}

BufferCoord Buffer::offset_coord(BufferCoord coord, CharCount offset)
{
    auto& line = m_lines[coord.line].content;
    auto character = std::max(0_char, std::min(line.char_count_to(coord.column) + offset,
                                               line.char_length() - 1));
    return {coord.line, line.byte_count_to(character)};
}

BufferCoord Buffer::offset_coord(BufferCoord coord, LineCount offset)
{
    auto character = m_lines[coord.line].content.char_count_to(coord.column);
    auto line = Kakoune::clamp(coord.line + offset, 0_line, line_count()-1);
    auto& content = m_lines[line].content;

    character = std::max(0_char, std::min(character, content.char_length() - 2));
    return {line, content.byte_count_to(character)};
}

String Buffer::string(BufferCoord begin, BufferCoord end) const
{
    String res;
    for (auto line = begin.line; line <= end.line and line < line_count(); ++line)
    {
        ByteCount start = 0;
        if (line == begin.line)
            start = begin.column;
        ByteCount count = -1;
        if (line == end.line)
            count = end.column - start;
        res += m_lines[line].content.substr(start, count);
    }
    return res;
}

// A Modification holds a single atomic modification to Buffer
struct Buffer::Modification
{
    enum Type { Insert, Erase };

    Type        type;
    BufferCoord coord;
    String      content;

    Modification(Type type, BufferCoord coord, String content)
        : type(type), coord(coord), content(std::move(content)) {}

    Modification inverse() const
    {
        Type inverse_type = Insert;
        switch (type)
        {
        case Insert: inverse_type = Erase;  break;
        case Erase:  inverse_type = Insert; break;
        default: kak_assert(false);
        }
        return {inverse_type, coord, content};
    }
};

class UndoGroupOptimizer
{
    static constexpr auto Insert = Buffer::Modification::Type::Insert;
    static constexpr auto Erase = Buffer::Modification::Type::Erase;

    static BufferCoord advance(BufferCoord coord, const String& str)
    {
        for (auto c : str)
        {
            if (c == '\n')
            {
                ++coord.line;
                coord.column = 0;
            }
            else
                ++coord.column;
        }
        return coord;
    }

    static ByteCount count_byte_to(BufferCoord pos, BufferCoord endpos, const String& str)
    {
        ByteCount count = 0;
        for (auto it = str.begin(); it != str.end() and pos != endpos; ++it)
        {
            if (*it == '\n')
            {
                ++pos.line;
                pos.column = 0;
            }
            else
                ++pos.column;
            ++count;
        }
        kak_assert(pos == endpos);
        return count;
    }

    static const ByteCount overlaps(const String& lhs, const String& rhs)
    {
        if (lhs.empty() or rhs.empty())
            return -1;

        char c = rhs.front();
        ByteCount pos = 0;
        while ((pos = (int)lhs.find_first_of(c, (int)pos)) != -1)
        {
            ByteCount i = pos, j = 0;
            while (i != lhs.length() and j != rhs.length() and lhs[i] == rhs[j])
                 ++i, ++j;
            if (i == lhs.length())
                break;
            ++pos;
        }
        return pos;
    }

    static bool merge_contiguous(Buffer::UndoGroup& undo_group)
    {
        bool progress = false;
        auto it = undo_group.begin();
        auto it_next = it+1;
        while (it_next != undo_group.end())
        {
            ByteCount pos;
            auto& coord = it->coord;
            auto& next_coord = it_next->coord;

            // reorders modification doing a kind of custom bubble sort
            // so we have a O(n²) worst case complexity of the undo group optimization
            if (next_coord < coord)
            {
                BufferCoord next_end = advance(next_coord, it_next->content);
                if (it_next->type == Insert)
                {
                    if (coord.line == next_coord.line)
                        coord.column += next_end.column - next_coord.column;
                    coord.line += next_end.line - next_coord.line;
                }
                else if (it->type == Insert and next_end > coord)
                {
                    ByteCount start = count_byte_to(next_coord, coord, it_next->content);
                    ByteCount len = std::min(it->content.length(), it_next->content.length() - start);
                    kak_assert(it_next->content.substr(start, len) == it->content.substr(0, len));
                    it->coord = it_next->coord;
                    it->content = it->content.substr(len);
                    it_next->content = it_next->content.substr(0,start) + it_next->content.substr(start + len);
                }
                else if (it->type == Erase and next_end >= coord)
                {
                    ByteCount start = count_byte_to(next_coord, coord, it_next->content);
                    it_next->content = it_next->content.substr(0, start) + it->content + it_next->content.substr(start);
                    it->coord = it_next->coord;
                    it->content.clear();
                }
                else
                {
                    if (next_end.line == coord.line)
                    {
                        coord.line = next_coord.line;
                        coord.column = next_coord.column + coord.column - next_end.column;
                    }
                    else
                        coord.line -= next_end.line - next_coord.line;
                }
                std::swap(*it, *it_next);
                progress = true;
            }

            kak_assert(coord <= next_coord);
            if (it->type == Erase and it_next->type == Erase and coord == next_coord)
            {
                it->content += it_next->content;
                it_next = undo_group.erase(it_next);
                progress = true;
            }
            else if (it->type == Insert and it_next->type == Insert and
                     is_in_range(next_coord, coord, advance(coord, it->content)))
            {
                ByteCount prefix_len = count_byte_to(coord, next_coord, it->content);
                it->content = it->content.substr(0, prefix_len) + it_next->content
                            + it->content.substr(prefix_len);
                it_next = undo_group.erase(it_next);
                progress = true;
            }
            else if (it->type == Insert and it_next->type == Erase and
                     next_coord < advance(coord, it->content))
            {
                ByteCount insert_len = it->content.length();
                ByteCount erase_len  = it_next->content.length();
                ByteCount prefix_len = count_byte_to(coord, next_coord, it->content);

                ByteCount suffix_len = insert_len - prefix_len;
                if (suffix_len >= erase_len)
                {
                    it->content = it->content.substr(0, prefix_len) + it->content.substr(prefix_len + erase_len);
                    it_next = undo_group.erase(it_next);
                }
                else
                {
                    it->content = it->content.substr(0, prefix_len);
                    it_next->content = it_next->content.substr(suffix_len);
                    ++it, ++it_next;
                }
                progress = true;
            }
            else if (it->type == Erase and it_next->type == Insert and coord == next_coord and
                     (pos = overlaps(it->content, it_next->content)) != -1)
            {
                ByteCount overlaps_len = it->content.length() - pos;
                it->content = it->content.substr(0, pos);
                it_next->coord = advance(it_next->coord, it_next->content.substr(0, overlaps_len));
                it_next->content = it_next->content.substr(overlaps_len);
                ++it, ++it_next;
                progress = true;
            }
            else
                ++it, ++it_next;
        }
        return progress;
    }

    static bool erase_empty(Buffer::UndoGroup& undo_group)
    {
        auto it = std::remove_if(begin(undo_group), end(undo_group),
                                 [](Buffer::Modification& m) { return m.content.empty(); });
        if (it != end(undo_group))
        {
            undo_group.erase(it, end(undo_group));
            return true;
        }
        return false;
    }
public:
    static void optimize(Buffer::UndoGroup& undo_group)
    {
        while (undo_group.size() > 1)
        {
            bool progress = false;
            progress |= merge_contiguous(undo_group);
            progress |= erase_empty(undo_group);
            if (not progress)
                break;
        }
    }
};

void Buffer::commit_undo_group()
{
    if (m_flags & Flags::NoUndo)
        return;

    UndoGroupOptimizer::optimize(m_current_undo_group);

    if (m_current_undo_group.empty())
        return;

    m_history.erase(m_history_cursor, m_history.end());

    m_history.push_back(std::move(m_current_undo_group));
    m_current_undo_group.clear();
    m_history_cursor = m_history.end();

    if (m_history.size() < m_last_save_undo_index)
        m_last_save_undo_index = -1;
}

bool Buffer::undo()
{
    commit_undo_group();

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

    kak_assert(m_current_undo_group.empty());

    for (const Modification& modification : *m_history_cursor)
        apply_modification(modification);

    ++m_history_cursor;
    return true;
}

void Buffer::check_invariant() const
{
#ifdef KAK_DEBUG
    ByteCount start = 0;
    kak_assert(not m_lines.empty());
    for (auto& line : m_lines)
    {
        kak_assert(line.start == start);
        kak_assert(line.length() > 0);
        kak_assert(line.content.back() == '\n');
        start += line.length();
    }
#endif
}

BufferCoord Buffer::do_insert(BufferCoord pos, const String& content)
{
    kak_assert(is_valid(pos));

    if (content.empty())
        return pos;

    ++m_timestamp;
    ByteCount offset = this->offset(pos);

    // all following lines advanced by length
    for (LineCount i = pos.line+1; i < line_count(); ++i)
        m_lines[i].start += content.length();

    BufferCoord begin;
    BufferCoord end;
    // if we inserted at the end of the buffer, we have created a new
    // line without inserting a '\n'
    if (is_end(pos))
    {
        ByteCount start = 0;
        for (ByteCount i = 0; i < content.length(); ++i)
        {
            if (content[i] == '\n')
            {
                m_lines.push_back({ m_timestamp, offset + start, content.substr(start, i + 1 - start) });
                start = i + 1;
            }
        }
        if (start != content.length())
            m_lines.push_back({ m_timestamp, offset + start, content.substr(start) });

        begin = pos.column == 0 ? pos : BufferCoord{ pos.line + 1, 0 };
        end = BufferCoord{ line_count()-1, m_lines.back().length() };
    }
    else
    {
        String prefix = m_lines[pos.line].content.substr(0, pos.column);
        String suffix = m_lines[pos.line].content.substr(pos.column);

        std::vector<Line> new_lines;

        ByteCount start = 0;
        for (ByteCount i = 0; i < content.length(); ++i)
        {
            if (content[i] == '\n')
            {
                String line_content = content.substr(start, i + 1 - start);
                if (start == 0)
                {
                    line_content = prefix + line_content;
                    new_lines.push_back({ m_timestamp, offset + start - prefix.length(),
                                          std::move(line_content) });
                }
                else
                    new_lines.push_back({ m_timestamp, offset + start, std::move(line_content) });
                start = i + 1;
            }
        }
        if (start == 0)
            new_lines.push_back({ m_timestamp, offset + start - prefix.length(), prefix + content + suffix });
        else if (start != content.length() or not suffix.empty())
            new_lines.push_back({ m_timestamp, offset + start, content.substr(start) + suffix });

        LineCount last_line = pos.line + new_lines.size() - 1;

        auto line_it = m_lines.begin() + (int)pos.line;
        *line_it = std::move(*new_lines.begin());

        m_lines.insert(line_it+1, std::make_move_iterator(new_lines.begin() + 1),
                       std::make_move_iterator(new_lines.end()));

        begin = pos;
        end = BufferCoord{ last_line, m_lines[last_line].length() - suffix.length() };
    }

    for (auto listener : m_change_listeners)
        listener->on_insert(*this, begin, end);
    return begin;
}

BufferCoord Buffer::do_erase(BufferCoord begin, BufferCoord end)
{
    kak_assert(is_valid(begin));
    kak_assert(is_valid(end));
    ++m_timestamp;
    const ByteCount length = distance(begin, end);
    String prefix = m_lines[begin.line].content.substr(0, begin.column);
    String suffix = m_lines[end.line].content.substr(end.column);
    Line new_line = { m_timestamp, m_lines[begin.line].start, prefix + suffix };

    BufferCoord next;
    if (new_line.length() != 0)
    {
        m_lines.erase(m_lines.begin() + (int)begin.line, m_lines.begin() + (int)end.line);
        m_lines[begin.line] = std::move(new_line);
        next = begin;
    }
    else
    {
        m_lines.erase(m_lines.begin() + (int)begin.line, m_lines.begin() + (int)end.line + 1);
        next = is_end(begin) ? end_coord() : BufferCoord{begin.line, 0};
    }

    for (LineCount i = begin.line+1; i < line_count(); ++i)
        m_lines[i].start -= length;

    for (auto listener : m_change_listeners)
        listener->on_erase(*this, begin, end);
    return next;
}

void Buffer::apply_modification(const Modification& modification)
{
    const String& content = modification.content;
    BufferCoord coord = modification.coord;

    kak_assert(is_valid(coord));
    // in modifications, end coords should be {line_count(), 0}
    kak_assert(coord != BufferCoord(line_count()-1, m_lines.back().length()));
    switch (modification.type)
    {
    case Modification::Insert:
    {
        do_insert(coord, content);
        break;
    }
    case Modification::Erase:
    {
        ByteCount count = content.length();
        BufferCoord end = advance(coord, count);
        kak_assert(string(coord, end) == content);
        do_erase(coord, end);
        break;
    }
    default:
        kak_assert(false);
    }
}

BufferIterator Buffer::insert(const BufferIterator& pos, String content)
{
    kak_assert(is_valid(pos.coord()));
    if (content.empty())
        return pos;

    if (pos == end() and content.back() != '\n')
        content += '\n';

    // for undo and redo purpose it is better to use one past last line rather
    // than one past last char coord.
    auto coord = pos == end() ? BufferCoord{line_count()} : pos.coord();
    if (not (m_flags & Flags::NoUndo))
        m_current_undo_group.emplace_back(Modification::Insert, coord, content);
    return {*this, do_insert(pos.coord(), content)};
}

BufferIterator Buffer::erase(BufferIterator begin, BufferIterator end)
{
    // do not erase last \n except if we erase from the start of a line
    if (end == this->end() and (begin.coord().column != 0 or begin == this->begin()))
        --end;

    if (begin == end)
        return begin;

    if (not (m_flags & Flags::NoUndo))
        m_current_undo_group.emplace_back(Modification::Erase, begin.coord(),
                                          string(begin.coord(), end.coord()));
    return {*this, do_erase(begin.coord(), end.coord())};
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
        commit_undo_group();

    m_flags &= ~Flags::New;
    size_t history_cursor_index = m_history_cursor - m_history.begin();
    if (m_last_save_undo_index != history_cursor_index)
    {
        ++m_timestamp;
        m_last_save_undo_index = history_cursor_index;
    }
    m_fs_timestamp = get_fs_timestamp(m_name);
}

BufferCoord Buffer::advance(BufferCoord coord, ByteCount count) const
{
    ByteCount off = Kakoune::clamp(offset(coord) + count, 0_byte, byte_count());
    auto it = std::upper_bound(m_lines.begin(), m_lines.end(), off,
                               [](ByteCount s, const Line& l) { return s < l.start; }) - 1;
    return { LineCount{ (int)(it - m_lines.begin()) }, off - it->start };
}

BufferCoord Buffer::char_next(BufferCoord coord) const
{
    if (coord.column < m_lines[coord.line].length() - 1)
    {
        auto& line = m_lines[coord.line].content;
        coord.column += utf8::codepoint_size(line.begin() + (int)coord.column);
        // Handle invalid utf-8
        if (coord.column >= line.length())
        {
            ++coord.line;
            coord.column = 0;
        }
    }
    else if (coord.line == m_lines.size() - 1)
        coord.column = m_lines.back().length();
    else
    {
        ++coord.line;
        coord.column = 0;
    }
    return coord;
}

BufferCoord Buffer::char_prev(BufferCoord coord) const
{
    kak_assert(is_valid(coord));
    if (is_end(coord))
        return coord = {(int)m_lines.size()-1, m_lines.back().length() - 1};
    else if (coord.column == 0)
    {
        if (coord.line > 0)
            coord.column = m_lines[--coord.line].length() - 1;
    }
    else
    {
        auto& line = m_lines[coord.line].content;
        coord.column = (int)(utf8::character_start(line.begin() + (int)coord.column - 1) - line.begin());
    }
    return coord;
}

time_t Buffer::fs_timestamp() const
{
    kak_assert(m_flags & Flags::File);
    return m_fs_timestamp;
}

void Buffer::set_fs_timestamp(time_t ts)
{
    kak_assert(m_flags & Flags::File);
    m_fs_timestamp = ts;
}

void Buffer::on_option_changed(const Option& option)
{
    run_hook_in_own_context("BufSetOption",
                            option.name() + "=" + option.get_as_string());
}

void Buffer::run_hook_in_own_context(const String& hook_name, const String& param)
{
    InputHandler hook_handler(*this, { Selection{} });
    m_hooks.run_hook(hook_name, param, hook_handler.context());
}

}
