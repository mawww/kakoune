#include "buffer.hh"

#include "assert.hh"
#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "client.hh"
#include "context.hh"
#include "diff.hh"
#include "file.hh"
#include "flags.hh"
#include "option_types.hh"
#include "ranges.hh"
#include "shared_string.hh"
#include "unit_tests.hh"
#include "utils.hh"
#include "window.hh"

#include <algorithm>

namespace Kakoune
{

struct ParsedLines
{
    BufferLines lines;
    ByteOrderMark bom = ByteOrderMark::None;
    EolFormat eolformat = EolFormat::Lf;
};

static ParsedLines parse_lines(StringView data)
{
    ParsedLines res;
    const char* pos = data.begin();
    if (data.substr(0, 3_byte) == "\xEF\xBB\xBF")
    {
        res.bom = ByteOrderMark::Utf8;
        pos = data.begin() + 3;
    }

    bool has_crlf = false, has_lf = false;
    for (auto it = pos; it != data.end(); ++it)
    {
        if (*it == '\n')
            ((it != pos and *(it-1) == '\r') ? has_crlf : has_lf) = true;
    }
    const bool crlf = has_crlf and not has_lf;
    res.eolformat = crlf ? EolFormat::Crlf : EolFormat::Lf;

    while (pos < data.end())
    {
        const char* eol = std::find(pos, data.end(), '\n');
        res.lines.emplace_back(StringData::create({{pos, eol - (crlf and eol != data.end() ? 1 : 0)}, "\n"}));
        pos = eol + 1;
    }

    return res;
}

static void apply_options(OptionManager& options, const ParsedLines& parsed_lines)
{
    options.get_local_option("eolformat").set(parsed_lines.eolformat);
    options.get_local_option("BOM").set(parsed_lines.bom);
}

Buffer::HistoryNode::HistoryNode(size_t id, HistoryNode* parent)
    : id{id}, parent(parent), timepoint{Clock::now()}
{}

Buffer::Buffer(String name, Flags flags, StringView data,
               timespec fs_timestamp)
    : Scope{GlobalScope::instance()},
      m_name{(flags & Flags::File) ? real_path(parse_filename(name)) : std::move(name)},
      m_display_name{(flags & Flags::File) ? compact_path(m_name) : m_name},
      m_flags{flags | Flags::NoUndo},
      m_history{m_next_history_id++, nullptr}, m_history_cursor{&m_history},
      m_last_save_history_cursor{&m_history},
      m_fs_timestamp{fs_timestamp.tv_sec, fs_timestamp.tv_nsec}
{
    ParsedLines parsed_lines = parse_lines(data);

    if (parsed_lines.lines.empty())
        parsed_lines.lines.emplace_back(StringData::create({"\n"}));

    #ifdef KAK_DEBUG
    for (auto& line : parsed_lines.lines)
        kak_assert(not (line->length == 0) and
                   line->data()[line->length-1] == '\n');
    #endif
    static_cast<BufferLines&>(m_lines) = std::move(parsed_lines.lines);

    m_changes.push_back({ Change::Insert, {0,0}, line_count() });

    apply_options(options(), parsed_lines);

    // now we may begin to record undo data
    if (not (flags & Flags::NoUndo))
        m_flags &= ~Flags::NoUndo;
}

void Buffer::on_registered()
{
    // Ignore debug buffer, as it can be created in many
    // corner cases (including while destroying the BufferManager
    // if a BufClose hooks triggers writing to it).
    if (m_flags & Flags::Debug)
        return;

    options().register_watcher(*this);

    run_hook_in_own_context("BufCreate", m_name);

    if (m_flags & Flags::File)
    {
        if (m_flags & Buffer::Flags::New)
            run_hook_in_own_context("BufNewFile", m_name);
        else
        {
            kak_assert(m_fs_timestamp != InvalidTime);
            run_hook_in_own_context("BufOpenFile", m_name);
        }
    }

    for (auto& option : options().flatten_options())
        on_option_changed(*option);
}

void Buffer::on_unregistered()
{
    if (m_flags & Flags::Debug)
        return;

    options().unregister_watcher(*this);
    run_hook_in_own_context("BufClose", m_name);
}

Buffer::~Buffer()
{
    m_values.clear();
}

bool Buffer::set_name(String name)
{
    Buffer* other = BufferManager::instance().get_buffer_ifp(name);
    if (other == nullptr or other == this)
    {
        if (m_flags & Flags::File)
        {
            m_name = real_path(name);
            m_display_name = compact_path(m_name);
        }
        else
        {
            m_name = std::move(name);
            m_display_name = m_name;
        }
        return true;
    }
    return false;
}

void Buffer::update_display_name()
{
    if (m_flags & Flags::File)
        m_display_name = compact_path(m_name);
}

BufferIterator Buffer::iterator_at(BufferCoord coord) const
{
    kak_assert(is_valid(coord));
    return {*this, coord};
}

BufferCoord Buffer::clamp(BufferCoord coord) const
{
    if (coord > back_coord())
        coord = back_coord();
    kak_assert(coord.line >= 0 and coord.line < line_count());
    ByteCount max_col = std::max(0_byte, m_lines[coord.line].length() - 1);
    coord.column = Kakoune::clamp(coord.column, 0_byte, max_col);
    return coord;
}

BufferCoord Buffer::offset_coord(BufferCoord coord, CharCount offset, ColumnCount, bool)
{
    StringView line = m_lines[coord.line];
    auto target = utf8::advance(&line[coord.column], offset < 0 ? line.begin() : line.end()-1, offset);
    return {coord.line, (int)(target - line.begin())};
}

BufferCoordAndTarget Buffer::offset_coord(BufferCoordAndTarget coord, LineCount offset, ColumnCount tabstop, bool avoid_eol)
{
    const auto column = coord.target == -1 ? get_column(*this, tabstop, coord) : coord.target;
    const auto line = Kakoune::clamp(coord.line + offset, 0_line, line_count()-1);
    const auto max_column = get_column(*this, tabstop, {line, m_lines[line].length()-1});
    const auto final_column = std::max(0_col, std::min(column, max_column - (avoid_eol ? 1 : 0)));
    return {line, get_byte_to_column(*this, tabstop, {line, final_column}), column};
}

String Buffer::string(BufferCoord begin, BufferCoord end) const
{
    String res;
    const auto last_line = std::min(end.line, line_count()-1);
    for (auto line = begin.line; line <= last_line; ++line)
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

    Type type;
    BufferCoord coord;
    StringDataPtr content;

    Modification inverse() const
    {
        return {type == Insert ? Erase : Insert, coord, content};
    }
};

void Buffer::reload(StringView data, timespec fs_timestamp)
{
    ParsedLines parsed_lines = parse_lines(data);

    if (parsed_lines.lines.empty())
        parsed_lines.lines.emplace_back(StringData::create({"\n"}));

    const bool record_undo = not (m_flags & Flags::NoUndo);

    commit_undo_group();

    if (not record_undo)
    {
        // Erase history about to be invalidated history
        m_history_cursor = &m_history;
        m_last_save_history_cursor = &m_history;
        m_history = HistoryNode{m_next_history_id++, nullptr};

        m_changes.push_back({ Change::Erase, {0,0}, line_count() });
        static_cast<BufferLines&>(m_lines) = std::move(parsed_lines.lines);
        m_changes.push_back({ Change::Insert, {0,0}, line_count() });
    }
    else
    {
        auto diff = find_diff(m_lines.begin(), m_lines.size(),
                              parsed_lines.lines.begin(), parsed_lines.lines.size(),
                              [](const StringDataPtr& lhs, const StringDataPtr& rhs)
                              { return lhs->strview() == rhs->strview(); });

        auto it = m_lines.begin();
        for (auto& d : diff)
        {
            if (d.mode == Diff::Keep)
                it += d.len;
            else if (d.mode == Diff::Add)
            {
                const LineCount cur_line = (int64_t)(it - m_lines.begin());

                for (LineCount line = 0; line < d.len; ++line)
                    m_current_undo_group.push_back({
                        Modification::Insert, cur_line + line,
                        parsed_lines.lines[(int64_t)(d.posB + line)]});

                m_changes.push_back({ Change::Insert, cur_line, cur_line + d.len });
                m_lines.insert(it, &parsed_lines.lines[d.posB], &parsed_lines.lines[d.posB + d.len]);
                it = m_lines.begin() + (int64_t)(cur_line + d.len);
            }
            else if (d.mode == Diff::Remove)
            {
                const LineCount cur_line = (int64_t)(it - m_lines.begin());

                for (LineCount line = d.len-1; line >= 0; --line)
                    m_current_undo_group.push_back({
                        Modification::Erase, cur_line + line,
                        m_lines.get_storage(cur_line + line)});

                it = m_lines.erase(it, it + d.len);
                m_changes.push_back({ Change::Erase, cur_line, cur_line + d.len });
            }
        }
    }

    commit_undo_group();

    apply_options(options(), parsed_lines);

    m_last_save_history_cursor = m_history_cursor;
    m_fs_timestamp = fs_timestamp;
}

void Buffer::commit_undo_group()
{
    if (m_flags & Flags::NoUndo)
        return;

    if (m_current_undo_group.empty())
        return;

    auto* node = new HistoryNode{m_next_history_id++, m_history_cursor.get()};
    node->undo_group = std::move(m_current_undo_group);
    m_current_undo_group.clear();

    m_history_cursor->childs.emplace_back(node);
    m_history_cursor->redo_child = node;
    m_history_cursor = node;
}

bool Buffer::undo(size_t count) noexcept
{
    commit_undo_group();

    if (not m_history_cursor->parent)
        return false;

    while (count-- != 0 and m_history_cursor->parent)
    {
        for (const Modification& modification : m_history_cursor->undo_group | reverse())
            apply_modification(modification.inverse());

        m_history_cursor = m_history_cursor->parent;
    }

    return true;
}

bool Buffer::redo(size_t count) noexcept
{
    if (not m_history_cursor->redo_child)
        return false;

    kak_assert(m_current_undo_group.empty());

    while (count-- != 0 and m_history_cursor->redo_child)
    {
        m_history_cursor = m_history_cursor->redo_child;

        for (const Modification& modification : m_history_cursor->undo_group)
            apply_modification(modification);
    }
    return true;
}

void Buffer::move_to(HistoryNode* history_node) noexcept
{
    commit_undo_group();

    auto find_lowest_common_parent = [](HistoryNode* a, HistoryNode* b) {
        auto depth_of = [](HistoryNode* node) {
            size_t depth = 0;
            for (; node->parent; node = node->parent.get())
                ++depth;
            return depth;
        };
        auto depthA = depth_of(a), depthB = depth_of(b);

        for (; depthA > depthB; --depthA)
            a = a->parent.get();
        for (; depthB > depthA; --depthB)
            b = b->parent.get();

        while (a != b)
        {
            a = a->parent.get();
            b = b->parent.get();
        }

        kak_assert(a == b and a != nullptr);
        return a;
    };

    auto parent = find_lowest_common_parent(m_history_cursor.get(), history_node);

    // undo up to common parent
    for (auto it = m_history_cursor.get(); it != parent; it = it->parent.get())
    {
        for (const Modification& modification : it->undo_group | reverse())
            apply_modification(modification.inverse());
    }

    static void (*apply_from_parent)(Buffer&, HistoryNode*, HistoryNode*) =
    [](Buffer& buffer, HistoryNode* parent, HistoryNode* node) {
        if (node == parent)
            return;

        apply_from_parent(buffer, parent, node->parent.get());

        node->parent->redo_child = node;

        for (const Modification& modification : node->undo_group)
            buffer.apply_modification(modification);
    };

    apply_from_parent(*this, parent, history_node);
    m_history_cursor = history_node;
}

template<typename Func>
Buffer::HistoryNode* Buffer::find_history_node(HistoryNode* node, const Func& func)
{
    if (func(node))
        return node;

    for (auto&& child : node->childs)
    {
        if (auto res = find_history_node(child.get(), func))
            return res;
    }

    return nullptr;
}

bool Buffer::move_to(size_t history_id) noexcept
{
    auto* target_node = find_history_node(&m_history, [history_id](auto* node)
                                          { return node->id == history_id; });
    if (not target_node)
        return false;

    move_to(target_node);
    return true;
}

size_t Buffer::current_history_id() const noexcept
{
    return m_history_cursor->id;
}

void Buffer::check_invariant() const
{
#ifdef KAK_DEBUG
    kak_assert(not m_lines.empty());
    for (auto& line : m_lines)
    {
        kak_assert(line->strview().length() > 0);
        kak_assert(line->strview().back() == '\n');
    }
#endif
}

BufferCoord Buffer::do_insert(BufferCoord pos, StringView content)
{
    kak_assert(is_valid(pos));

    if (content.empty())
        return pos;

    const bool at_end = is_end(pos);
    const bool append_lines = at_end and (m_lines.empty() or byte_at(back_coord()) == '\n');

    const StringView prefix = append_lines ?
        StringView{} : m_lines[pos.line].substr(0, pos.column);
    const StringView suffix = at_end ?
        StringView{} : m_lines[pos.line].substr(pos.column);

    LineList new_lines;
    ByteCount start = 0;
    for (ByteCount i = 0; i < content.length(); ++i)
    {
        if (content[i] == '\n')
        {
            StringView line = content.substr(start, i + 1 - start);
            new_lines.push_back(StringData::create(start == 0 ? prefix + line : line));
            start = i + 1;
        }
    }
    if (start == 0)
        new_lines.push_back(StringData::create({prefix, content, suffix}));
    else if (start != content.length() or not suffix.empty())
        new_lines.push_back(StringData::create({content.substr(start), suffix}));

    auto line_it = m_lines.begin() + (int64_t)pos.line;
    auto new_lines_it = new_lines.begin();
    if (not append_lines) // replace first line with new first line
        *line_it++ = std::move(*new_lines_it++);

    m_lines.insert(line_it,
                   std::make_move_iterator(new_lines_it),
                   std::make_move_iterator(new_lines.end()));

    const LineCount last_line = pos.line + new_lines.size() - 1;
    const auto end = at_end ? line_count()
                            : BufferCoord{ last_line, m_lines[last_line].length() - suffix.length() };

    m_changes.push_back({ Change::Insert, pos, end });
    return pos;
}

BufferCoord Buffer::do_erase(BufferCoord begin, BufferCoord end)
{
    if (begin == end)
        return begin;

    kak_assert(is_valid(begin));
    kak_assert(is_valid(end));
    StringView prefix = m_lines[begin.line].substr(0, begin.column);
    StringView suffix = end.line == line_count() ? StringView{} : m_lines[end.line].substr(end.column);

    BufferCoord next;
    if (not prefix.empty() or not suffix.empty())
    {
        auto new_line = StringData::create({prefix, suffix});
        m_lines.erase(m_lines.begin() + (int64_t)begin.line, m_lines.begin() + (int64_t)end.line);
        m_lines.get_storage(begin.line) = std::move(new_line);
        next = begin;
    }
    else
    {
        m_lines.erase(m_lines.begin() + (int64_t)begin.line, m_lines.begin() + (int64_t)end.line);
        next = begin.line;
    }

    m_changes.push_back({ Change::Erase, begin, end });
    return next;
}

void Buffer::apply_modification(const Modification& modification)
{
    StringView content = modification.content->strview();
    BufferCoord coord = modification.coord;

    kak_assert(is_valid(coord));
    switch (modification.type)
    {
    case Modification::Insert:
        do_insert(coord, content);
        break;
    case Modification::Erase:
    {
        auto end = advance(coord, content.length());
        kak_assert(string(coord, end) == content);
        do_erase(coord, end);
        break;
    }
    default:
        kak_assert(false);
    }
}

BufferCoord Buffer::insert(BufferCoord pos, StringView content)
{
    kak_assert(is_valid(pos));
    if (content.empty())
        return pos;

    StringDataPtr real_content;
    if (is_end(pos) and content.back() != '\n')
        real_content = intern(content + "\n");
    else
        real_content = intern(content);

    // for undo and redo purpose it is better to use one past last line rather
    // than one past last char coord.
    auto coord = is_end(pos) ? line_count() : pos;
    if (not (m_flags & Flags::NoUndo))
        m_current_undo_group.push_back({Modification::Insert, coord, real_content});
    return do_insert(pos, real_content->strview());
}

BufferCoord Buffer::erase(BufferCoord begin, BufferCoord end)
{
    kak_assert(is_valid(begin) and is_valid(end));
    // do not erase last \n except if we erase from the start of a line, and normalize
    // end coord
    if (is_end(end))
        end = (begin.column != 0 or begin == BufferCoord{0,0}) ? prev(end) : end_coord();

    if (begin >= end) // use >= to handle case where begin is {line_count}
        return begin;

    if (not (m_flags & Flags::NoUndo))
        m_current_undo_group.push_back({Modification::Erase, begin,
                                        intern(string(begin, end))});
    return do_erase(begin, end);
}

BufferCoord Buffer::replace(BufferCoord begin, BufferCoord end, StringView content)
{
    if (is_end(end) and not content.empty() and content.back() == '\n')
    {
        end = back_coord();
        content = content.substr(0, content.length() - 1);
    }

    auto pos = erase(begin, end);
    return insert(pos, content);
}

bool Buffer::is_modified() const
{
    return m_flags & Flags::File and
           (m_history_cursor != m_last_save_history_cursor or
            not m_current_undo_group.empty());
}

void Buffer::notify_saved()
{
    if (not m_current_undo_group.empty())
        commit_undo_group();

    m_flags &= ~Flags::New;
    m_last_save_history_cursor = m_history_cursor;
    m_fs_timestamp = get_fs_timestamp(m_name);
}

BufferCoord Buffer::advance(BufferCoord coord, ByteCount count) const
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

BufferCoord Buffer::char_next(BufferCoord coord) const
{
    if (coord.column < m_lines[coord.line].length() - 1)
    {
        auto line = m_lines[coord.line];
        auto column = coord.column + utf8::codepoint_size(line[coord.column]);
        if (column >= line.length()) // Handle invalid utf-8
            return { coord.line + 1, 0 };
        return { coord.line, column };
    }
    return { coord.line + 1, 0 };
}

BufferCoord Buffer::char_prev(BufferCoord coord) const
{
    kak_assert(is_valid(coord));
    if (coord.column == 0)
        return { coord.line - 1, m_lines[coord.line - 1].length() - 1 };

    auto line = m_lines[coord.line];
    auto column = (int64_t)(utf8::character_start(line.begin() + (int64_t)coord.column - 1, line.begin()) - line.begin());
    return { coord.line, column };
}

timespec Buffer::fs_timestamp() const
{
    kak_assert(m_flags & Flags::File);
    return m_fs_timestamp;
}

void Buffer::set_fs_timestamp(timespec ts)
{
    kak_assert(m_flags & Flags::File);
    m_fs_timestamp = ts;
}

void Buffer::on_option_changed(const Option& option)
{
    if (option.name() == "readonly")
    {
        if (option.get<bool>())
            m_flags |= Flags::ReadOnly;
        else
            m_flags &= ~Flags::ReadOnly;
    }
    run_hook_in_own_context("BufSetOption",
                            format("{}={}", option.name(), option.get_as_string()));
}

void Buffer::run_hook_in_own_context(StringView hook_name, StringView param, String client_name)
{
    if (m_flags & Buffer::Flags::NoHooks)
        return;

    InputHandler hook_handler{{ *this, Selection{} },
                              Context::Flags::Transient,
                              std::move(client_name)};
    hooks().run_hook(hook_name, param, hook_handler.context());
}

BufferCoord Buffer::last_modification_coord() const
{
    if (m_history_cursor.get() == &m_history)
        return {};
    return m_history_cursor->undo_group.back().coord;
}

String Buffer::debug_description() const
{
    size_t content_size = 0;
    for (auto& line : m_lines)
        content_size += (int64_t)line->strview().length();

    static size_t (*count_mem)(const HistoryNode&) = [](const HistoryNode& node) {
        size_t size = node.undo_group.size() * sizeof(Modification);
        for (auto& child : node.childs)
            size += count_mem(*child);
        return size;
    };
    const size_t additional_size = count_mem(m_history) +
        m_changes.size() * sizeof(Change);

    return format("{}\nFlags: {}{}{}{}\nUsed mem: content={} additional={}\n",
                  display_name(),
                  (m_flags & Flags::File) ? "File (" + name() + ") " : "",
                  (m_flags & Flags::New) ? "New " : "",
                  (m_flags & Flags::Fifo) ? "Fifo " : "",
                  (m_flags & Flags::NoUndo) ? "NoUndo " : "",
                  content_size, additional_size);
}

UnitTest test_parse_line{[]
{
    {
        auto lines = parse_lines("foo\nbar\nbaz\n");
        kak_assert(lines.eolformat == EolFormat::Lf);
        kak_assert(lines.bom == ByteOrderMark::None);
        kak_assert(lines.lines.size() == 3);
        kak_assert(lines.lines[0]->strview() == "foo\n");
        kak_assert(lines.lines[1]->strview() == "bar\n");
        kak_assert(lines.lines[2]->strview() == "baz\n");
    }

    {
        auto lines = parse_lines("\xEF\xBB\xBF" "foo\nbar\r\nbaz");
        kak_assert(lines.eolformat == EolFormat::Lf);
        kak_assert(lines.bom == ByteOrderMark::Utf8);
        kak_assert(lines.lines.size() == 3);
        kak_assert(lines.lines[0]->strview() == "foo\n");
        kak_assert(lines.lines[1]->strview() == "bar\r\n");
        kak_assert(lines.lines[2]->strview() == "baz\n");
    }

    {
        auto lines = parse_lines("foo\r\nbar\r\nbaz\r\n");
        kak_assert(lines.eolformat == EolFormat::Crlf);
        kak_assert(lines.bom == ByteOrderMark::None);
        kak_assert(lines.lines.size() == 3);
        kak_assert(lines.lines[0]->strview() == "foo\n");
        kak_assert(lines.lines[1]->strview() == "bar\n");
        kak_assert(lines.lines[2]->strview() == "baz\n");
    }
}};

UnitTest test_buffer{[]()
{
    Buffer empty_buffer("empty", Buffer::Flags::None, {});

    Buffer buffer("test", Buffer::Flags::None, "allo ?\nmais que fais la police\n hein ?\n youpi\n");
    kak_assert(buffer.line_count() == 4);

    BufferIterator pos = buffer.begin();
    kak_assert(*pos == 'a');
    pos += 6;
    kak_assert(pos.coord() == BufferCoord{0, 6});
    ++pos;
    kak_assert(pos.coord() == BufferCoord{1, 0});
    --pos;
    kak_assert(pos.coord() == BufferCoord{0, 6});
    pos += 1;
    kak_assert(pos.coord() == BufferCoord{1, 0});
    buffer.insert(pos.coord(), "tchou kanaky\n");
    kak_assert(buffer.line_count() == 5);
    BufferIterator pos2 = buffer.end();
    pos2 -= 9;
    kak_assert(*pos2 == '?');

    String str = buffer.string({ 4, 1 }, buffer.next({ 4, 5 }));
    kak_assert(str == "youpi");

    // check insert at end behaviour: auto add end of line if necessary
    pos = buffer.end()-1;
    buffer.insert(pos.coord(), "tchou");
    kak_assert(buffer.string(pos.coord(), buffer.end_coord()) == "tchou\n"_sv);

    pos = buffer.end()-1;
    buffer.insert(buffer.end_coord(), "kanaky\n");
    kak_assert(buffer.string((pos+1).coord(), buffer.end_coord()) == "kanaky\n"_sv);

    buffer.commit_undo_group();
    buffer.erase((pos+1).coord(), buffer.end_coord());
    buffer.insert(buffer.end_coord(), "mutch\n");
    buffer.commit_undo_group();
    buffer.undo();
    kak_assert(buffer.string(buffer.advance(buffer.end_coord(), -7), buffer.end_coord()) == "kanaky\n"_sv);
    buffer.redo();
    kak_assert(buffer.string(buffer.advance(buffer.end_coord(), -6), buffer.end_coord()) == "mutch\n"_sv);
}};

UnitTest test_undo{[]()
{
    Buffer buffer("test", Buffer::Flags::None, "allo ?\nmais que fais la police\n hein ?\n youpi\n");
    auto pos = buffer.insert(buffer.end_coord(), "kanaky\n"); // change 1
    buffer.commit_undo_group();
    buffer.erase(pos, buffer.end_coord());                    // change 2
    buffer.commit_undo_group();
    buffer.insert(2_line, "tchou\n");                         // change 3
    buffer.commit_undo_group();
    buffer.undo();
    buffer.insert(2_line, "mutch\n");                         // change 4
    buffer.commit_undo_group();
    buffer.erase({2, 1}, {2, 5});                             // change 5
    buffer.commit_undo_group();
    buffer.undo(2);
    buffer.redo(2);
    buffer.undo();
    buffer.replace(2_line, buffer.end_coord(), "foo");        // change 6
    buffer.commit_undo_group();

    kak_assert((int64_t)buffer.line_count() == 3);
    kak_assert(buffer[0_line] == "allo ?\n");
    kak_assert(buffer[1_line] == "mais que fais la police\n");
    kak_assert(buffer[2_line] == "foo\n");

    buffer.move_to(3);
    kak_assert((int64_t)buffer.line_count() == 5);
    kak_assert(buffer[0_line] == "allo ?\n");
    kak_assert(buffer[1_line] == "mais que fais la police\n");
    kak_assert(buffer[2_line] == "tchou\n");
    kak_assert(buffer[3_line] == " hein ?\n");
    kak_assert(buffer[4_line] == " youpi\n");

    buffer.move_to(4);
    kak_assert((int64_t)buffer.line_count() == 5);
    kak_assert(buffer[0_line] == "allo ?\n");
    kak_assert(buffer[1_line] == "mais que fais la police\n");
    kak_assert(buffer[2_line] == "mutch\n");
    kak_assert(buffer[3_line] == " hein ?\n");
    kak_assert(buffer[4_line] == " youpi\n");
}};

}
