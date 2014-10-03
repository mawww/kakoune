#include "buffer.hh"

#include "assert.hh"
#include "buffer_manager.hh"
#include "client.hh"
#include "context.hh"
#include "file.hh"
#include "interned_string.hh"
#include "utils.hh"
#include "window.hh"

#include <algorithm>

namespace Kakoune
{

Buffer::Buffer(String name, Flags flags, std::vector<String> lines,
               time_t fs_timestamp)
    : m_name(flags & Flags::File ? real_path(parse_filename(name)) : std::move(name)),
      m_flags(flags | Flags::NoUndo),
      m_history(), m_history_cursor(m_history.begin()),
      m_last_save_undo_index(0),
      m_fs_timestamp(fs_timestamp),
      m_hooks(GlobalHooks::instance()),
      m_options(GlobalOptions::instance()),
      m_keymaps(GlobalKeymaps::instance())
{
    BufferManager::instance().register_buffer(*this);
    m_options.register_watcher(*this);

    if (lines.empty())
        lines.emplace_back("\n");

    m_lines.reserve(lines.size());
    for (auto& line : lines)
    {
        kak_assert(not line.empty() and line.back() == '\n');
        m_lines.emplace_back(std::move(line));
    }

    m_changes.push_back({ Change::Insert, {0,0}, line_count(), true });

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
}

void Buffer::reload(std::vector<String> lines, time_t fs_timestamp)
{
    m_changes.push_back({ Change::Erase, {0,0}, back_coord(), true });

    m_history.clear();
    m_current_undo_group.clear();
    m_history_cursor = m_history.begin();
    m_last_save_undo_index = 0;
    m_lines.clear();

    if (lines.empty())
        lines.emplace_back("\n");

    m_lines.reserve(lines.size());
    for (auto& line : lines)
    {
        kak_assert(not line.empty() and line.back() == '\n');
        m_lines.emplace_back(std::move(line));
    }
    m_fs_timestamp = fs_timestamp;

    m_changes.push_back({ Change::Insert, {0,0}, back_coord(), true });
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

BufferIterator Buffer::iterator_at(ByteCoord coord) const
{
    return is_end(coord) ? end() : BufferIterator(*this, clamp(coord));
}

ByteCoord Buffer::clamp(ByteCoord coord) const
{
    if (m_lines.empty())
        return ByteCoord{};

    coord.line = Kakoune::clamp(coord.line, 0_line, line_count() - 1);
    ByteCount max_col = std::max(0_byte, m_lines[coord.line].length() - 1);
    coord.column = Kakoune::clamp(coord.column, 0_byte, max_col);
    return coord;
}

ByteCoord Buffer::offset_coord(ByteCoord coord, CharCount offset)
{
    auto& line = m_lines[coord.line];
    auto character = std::max(0_char, std::min(line.char_count_to(coord.column) + offset,
                                               line.char_length() - 1));
    return {coord.line, line.byte_count_to(character)};
}

ByteCoordAndTarget Buffer::offset_coord(ByteCoordAndTarget coord, LineCount offset)
{
    auto character = coord.target == -1 ? m_lines[coord.line].char_count_to(coord.column) : coord.target;
    auto line = Kakoune::clamp(coord.line + offset, 0_line, line_count()-1);
    auto& content = m_lines[line];

    character = std::max(0_char, std::min(character, content.char_length() - 2));
    return {line, content.byte_count_to(character), character};
}

String Buffer::string(ByteCoord begin, ByteCoord end) const
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
        res += m_lines[line].substr(start, count);
    }
    return res;
}

// A Modification holds a single atomic modification to Buffer
struct Buffer::Modification
{
    enum Type { Insert, Erase };

    Type      type;
    ByteCoord coord;
    InternedString content;

    Modification(Type type, ByteCoord coord, InternedString content)
        : type(type), coord(coord), content(std::move(content)) {}

    Modification inverse() const
    {
        return {type == Insert ? Erase : Insert, coord, content};
    }
};


void Buffer::commit_undo_group()
{
    if (m_flags & Flags::NoUndo)
        return;

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
    kak_assert(not m_lines.empty());
    for (auto& line : m_lines)
    {
        kak_assert(line.length() > 0);
        kak_assert(line.back() == '\n');
    }
#endif
}

ByteCoord Buffer::do_insert(ByteCoord pos, StringView content)
{
    kak_assert(is_valid(pos));

    if (content.empty())
        return pos;

    ByteCoord begin;
    ByteCoord end;
    bool at_end = false;
    // if we inserted at the end of the buffer, we have created a new
    // line without inserting a '\n'
    if (is_end(pos))
    {
        ByteCount start = 0;
        for (ByteCount i = 0; i < content.length(); ++i)
        {
            if (content[i] == '\n')
            {
                m_lines.push_back(content.substr(start, i + 1 - start));
                start = i + 1;
            }
        }
        if (start != content.length())
            m_lines.push_back(content.substr(start));

        begin = pos.column == 0 ? pos : ByteCoord{ pos.line + 1, 0 };
        end = ByteCoord{ line_count(), 0 };
        at_end = true;
    }
    else
    {
        StringView prefix = m_lines[pos.line].substr(0, pos.column);
        StringView suffix = m_lines[pos.line].substr(pos.column);

        std::vector<InternedString> new_lines;

        ByteCount start = 0;
        for (ByteCount i = 0; i < content.length(); ++i)
        {
            if (content[i] == '\n')
            {
                StringView line_content = content.substr(start, i + 1 - start);
                if (start == 0)
                    new_lines.emplace_back(prefix + line_content);
                else
                    new_lines.push_back(line_content);
                start = i + 1;
            }
        }
        if (start == 0)
            new_lines.emplace_back(prefix + content + suffix);
        else if (start != content.length() or not suffix.empty())
            new_lines.emplace_back(content.substr(start) + suffix);

        LineCount last_line = pos.line + new_lines.size() - 1;

        auto line_it = m_lines.begin() + (int)pos.line;
        *line_it = std::move(*new_lines.begin());

        m_lines.insert(line_it+1, std::make_move_iterator(new_lines.begin() + 1),
                       std::make_move_iterator(new_lines.end()));

        begin = pos;
        end = ByteCoord{ last_line, m_lines[last_line].length() - suffix.length() };
    }

    m_changes.push_back({ Change::Insert, begin, end, at_end });
    return begin;
}

ByteCoord Buffer::do_erase(ByteCoord begin, ByteCoord end)
{
    kak_assert(is_valid(begin));
    kak_assert(is_valid(end));
    StringView prefix = m_lines[begin.line].substr(0, begin.column);
    StringView suffix = m_lines[end.line].substr(end.column);
    String new_line = prefix + suffix;

    ByteCoord next;
    if (new_line.length() != 0)
    {
        m_lines.erase(m_lines.begin() + (int)begin.line, m_lines.begin() + (int)end.line);
        m_lines[begin.line] = InternedString(new_line);
        next = begin;
    }
    else
    {
        m_lines.erase(m_lines.begin() + (int)begin.line, m_lines.begin() + (int)end.line + 1);
        next = is_end(begin) ? end_coord() : ByteCoord{begin.line, 0};
    }

    m_changes.push_back({ Change::Erase, begin, end, is_end(begin) });
    return next;
}

void Buffer::apply_modification(const Modification& modification)
{
    StringView content = modification.content;
    ByteCoord coord = modification.coord;

    kak_assert(is_valid(coord));
    // in modifications, end coords should be {line_count(), 0}
    kak_assert(coord != ByteCoord(line_count()-1, m_lines.back().length()));
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
        ByteCoord end = advance(coord, count);
        kak_assert(string(coord, end) == content);
        do_erase(coord, end);
        break;
    }
    default:
        kak_assert(false);
    }
}

BufferIterator Buffer::insert(const BufferIterator& pos, StringView content)
{
    kak_assert(is_valid(pos.coord()));
    if (content.empty())
        return pos;

    InternedString real_content;
    if (pos == end() and content.back() != '\n')
        real_content = InternedString(content + "\n");
    else
        real_content = content;

    // for undo and redo purpose it is better to use one past last line rather
    // than one past last char coord.
    auto coord = pos == end() ? ByteCoord{line_count()} : pos.coord();
    if (not (m_flags & Flags::NoUndo))
        m_current_undo_group.emplace_back(Modification::Insert, coord, real_content);
    return {*this, do_insert(pos.coord(), real_content)};
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
                                          InternedString(string(begin.coord(), end.coord())));
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
        m_last_save_undo_index = history_cursor_index;
    m_fs_timestamp = get_fs_timestamp(m_name);
}

ByteCoord Buffer::advance(ByteCoord coord, ByteCount count) const
{
    if (count > 0)
    {
        auto line = coord.line;
        count += coord.column;
        while (count >= m_lines[line].length())
        {
            count -= m_lines[line++].length();
            if (line == line_count())
                return end_coord();
        }
        return { line, count };
    }
    else if (count < 0)
    {
        auto line = coord.line;
        count += coord.column;
        while (count < 0)
        {
            count += m_lines[--line].length();
            if (line < 0)
                return {0, 0};
        }
        return { line, count };
    }
    return coord;
}

ByteCoord Buffer::char_next(ByteCoord coord) const
{
    if (coord.column < m_lines[coord.line].length() - 1)
    {
        auto line = m_lines[coord.line];
        coord.column += utf8::codepoint_size(line[(int)coord.column]);
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

ByteCoord Buffer::char_prev(ByteCoord coord) const
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
        auto line = m_lines[coord.line];
        coord.column = (int)(utf8::character_start(line.begin() + (int)coord.column - 1, line.begin()) - line.begin());
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
    InputHandler hook_handler({ *this, Selection{} });
    m_hooks.run_hook(hook_name, param, hook_handler.context());
}

ByteCoord Buffer::last_modification_coord() const
{
    if (m_history.empty())
        return {};
    return m_history.back().back().coord;
}

String Buffer::debug_description() const
{
    String res = display_name() + "\n";

    res += "  Flags: ";
    if (m_flags & Flags::File)
        res += "File (" + name() + ") ";
    if (m_flags & Flags::New)
        res += "New ";
    if (m_flags & Flags::Fifo)
        res += "Fifo ";
    if (m_flags & Flags::NoUndo)
        res += "NoUndo ";
    res += "\n";

    size_t content_size = 0;
    for (auto& line : m_lines)
        content_size += (int)line.length();

    size_t additional_size = 0;
    for (auto& undo_group : m_history)
        additional_size += undo_group.size() * sizeof(Modification);
    additional_size += m_changes.size() * sizeof(Change);

    res += "  Used mem: content=" + to_string(content_size) +
           " additional=" + to_string(additional_size) + "\n";
    return res;
}

}
