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

Buffer::HistoryNode::HistoryNode(HistoryId parent)
    : parent{parent}, committed{Clock::now()}
{}

Buffer::Buffer(String name, Flags flags, BufferLines lines,
               ByteOrderMark bom, EolFormat eolformat,
               FsStatus fs_status)
    : Scope{GlobalScope::instance()},
      m_name{(flags & Flags::File) ? real_path(parse_filename(name)) : std::move(name)},
      m_display_name{(flags & Flags::File) ? compact_path(m_name) : m_name},
      m_flags{flags | Flags::NoUndo},
      m_history{{HistoryId::Invalid}},
      m_history_id{HistoryId::First},
      m_last_save_history_id{HistoryId::First},
      m_fs_status{fs_status}
{
    #ifdef KAK_DEBUG
    for (auto& line : lines)
        kak_assert(not (line->length == 0) and
                   line->data()[line->length-1] == '\n');
    #endif
    static_cast<BufferLines&>(m_lines) = std::move(lines);

    m_changes.push_back({ Change::Insert, {0,0}, line_count() });

    options().get_local_option("eolformat").set(eolformat);
    options().get_local_option("BOM").set(bom);

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

    if (m_flags & Buffer::Flags::NoHooks)
    {
        on_option_changed(options()["readonly"]);
        return;
    }

    run_hook_in_own_context(Hook::BufCreate, m_name);

    if (m_flags & Flags::File)
    {
        if (m_flags & Buffer::Flags::New)
            run_hook_in_own_context(Hook::BufNewFile, m_name);
        else
        {
            kak_assert(m_fs_status.timestamp != InvalidTime);
            run_hook_in_own_context(Hook::BufOpenFile, m_name);
        }
    }

    for (auto& option : options().flatten_options()
                      | transform(&std::unique_ptr<Option>::get)
                      | gather<Vector<Option*>>())
        on_option_changed(*option);
}

void Buffer::on_unregistered()
{
    if (m_flags & Flags::Debug)
        return;

    options().unregister_watcher(*this);
    run_hook_in_own_context(Hook::BufClose, m_name);
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
            if (m_flags & Buffer::Flags::File and not file_exists(m_name))
            {
                m_flags |= Buffer::Flags::New;
                m_last_save_history_id = HistoryId::Invalid;
            }
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

void Buffer::throw_if_read_only() const
{
    if (m_flags & Flags::ReadOnly)
        throw runtime_error("buffer is read-only");
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

BufferCoord Buffer::offset_coord(BufferCoord coord, CharCount offset, ColumnCount) const
{
    return utf8::advance(iterator_at(coord), offset < 0 ? begin() : end()-1, offset).coord();
}

BufferCoordAndTarget Buffer::offset_coord(BufferCoordAndTarget coord, LineCount offset, ColumnCount tabstop) const
{
    const auto column = coord.target == -1 ? get_column(*this, tabstop, coord) : coord.target;
    const bool avoid_eol = coord.target < max_column;
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

Buffer::Modification Buffer::Modification::inverse() const
{
    return {type == Insert ? Erase : Insert, coord, content};
}

void Buffer::reload(BufferLines lines, ByteOrderMark bom, EolFormat eolformat, FsStatus fs_status)
{
    const bool record_undo = not (m_flags & Flags::NoUndo);

    commit_undo_group();

    if (not record_undo)
    {
        // Erase history about to be invalidated history
        m_history_id = HistoryId::First;
        m_last_save_history_id = HistoryId::First;
        m_history = {HistoryNode{HistoryId::Invalid}};

        m_changes.push_back({ Change::Erase, {0,0}, line_count() });
        static_cast<BufferLines&>(m_lines) = std::move(lines);
        m_changes.push_back({ Change::Insert, {0,0}, line_count() });
    }
    else
    {
        Vector<Diff> diff;
        for_each_diff(m_lines.begin(), m_lines.size(),
                      lines.begin(), lines.size(),
                      [&diff](DiffOp op, int len)
                      { diff.push_back({op, len}); },
                      [](const StringDataPtr& lhs, const StringDataPtr& rhs)
                      { return lhs->strview() == rhs->strview(); });

        auto it = m_lines.begin();
        auto new_it = lines.begin();
        for (auto& d : diff)
        {
            if (d.op == DiffOp::Keep)
            {
                it += d.len;
                new_it += d.len;
            }
            else if (d.op == DiffOp::Add)
            {
                const LineCount cur_line = (int)(it - m_lines.begin());

                for (LineCount line = 0; line < d.len; ++line)
                    m_current_undo_group.push_back({Modification::Insert, cur_line + line, *(new_it + (int)line)});

                m_changes.push_back({Change::Insert, cur_line, cur_line + d.len});
                m_lines.insert(it, new_it, new_it + d.len);
                it = m_lines.begin() + (int)(cur_line + d.len);
                new_it += d.len;
            }
            else if (d.op == DiffOp::Remove)
            {
                const LineCount cur_line = (int)(it - m_lines.begin());

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

    options().get_local_option("eolformat").set(eolformat);
    options().get_local_option("BOM").set(bom);


    m_last_save_history_id = m_history_id;
    m_fs_status = fs_status;
}

void Buffer::commit_undo_group()
{
    if (m_flags & Flags::NoUndo)
        return;

    if (m_current_undo_group.empty())
        return;

    const HistoryId id = next_history_id();
    m_history.push_back({m_history_id});
    m_history.back().undo_group = std::move(m_current_undo_group);
    m_current_undo_group.clear();
    current_history_node().redo_child = id;
    m_history_id = id;
}

bool Buffer::undo(size_t count)
{
    throw_if_read_only();

    commit_undo_group();

    if (current_history_node().parent == HistoryId::Invalid)
        return false;

    while (count-- != 0 and current_history_node().parent != HistoryId::Invalid)
    {
        for (const Modification& modification : current_history_node().undo_group | reverse())
            apply_modification(modification.inverse());

        m_history_id = current_history_node().parent;
    }

    return true;
}

bool Buffer::redo(size_t count)
{
    throw_if_read_only();

    if (current_history_node().redo_child == HistoryId::Invalid)
        return false;

    kak_assert(m_current_undo_group.empty());

    while (count-- != 0 and current_history_node().redo_child != HistoryId::Invalid)
    {
        m_history_id = current_history_node().redo_child;

        for (const Modification& modification : current_history_node().undo_group)
            apply_modification(modification);
    }
    return true;
}

bool Buffer::move_to(HistoryId id)
{
    if (id >= next_history_id())
        return false;

    throw_if_read_only();

    commit_undo_group();

    auto find_lowest_common_parent = [this](HistoryId a, HistoryId b) {
        auto depth_of = [this](HistoryId id) {
            size_t depth = 0;
            for (; history_node(id).parent != HistoryId::Invalid; id = history_node(id).parent)
                ++depth;
            return depth;
        };
        auto depthA = depth_of(a), depthB = depth_of(b);

        for (; depthA > depthB; --depthA)
            a = history_node(a).parent;
        for (; depthB > depthA; --depthB)
            b = history_node(b).parent;

        while (a != b)
        {
            a = history_node(a).parent;
            b = history_node(b).parent;
        }

        kak_assert(a == b and a != HistoryId::Invalid);
        return a;
    };

    auto parent = find_lowest_common_parent(m_history_id, id);

    // undo up to common parent
    for (auto id = m_history_id; id != parent; id = history_node(id).parent)
    {
        for (const Modification& modification : history_node(id).undo_group | reverse())
            apply_modification(modification.inverse());
    }

    static void (*apply_from_parent)(Buffer&, HistoryId, HistoryId) =
    [](Buffer& buffer, HistoryId parent, HistoryId id) {
        if (id == parent)
            return;

        auto& node = buffer.history_node(id);
        apply_from_parent(buffer, parent, node.parent);

        buffer.history_node(node.parent).redo_child = id;

        for (const Modification& modification : node.undo_group)
            buffer.apply_modification(modification);
    };

    apply_from_parent(*this, parent, id);
    m_history_id = id;
    return true;
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

BufferRange Buffer::do_insert(BufferCoord pos, StringView content)
{
    kak_assert(is_valid(pos));

    if (content.empty())
        return {pos, pos};

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
            new_lines.push_back(start == 0 ? StringData::create({prefix, line}) : StringData::create({line}));
            start = i + 1;
        }
    }
    if (start == 0)
        new_lines.push_back(StringData::create({prefix, content, suffix}));
    else if (start != content.length() or not suffix.empty())
        new_lines.push_back(StringData::create({content.substr(start), suffix}));

    auto line_it = m_lines.begin() + (int)pos.line;
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
    return {pos, end};
}

BufferCoord Buffer::do_erase(BufferCoord begin, BufferCoord end)
{
    if (begin == end)
        return begin;

    kak_assert(is_valid(begin));
    kak_assert(is_valid(end));
    StringView prefix = m_lines[begin.line].substr(0, begin.column);
    StringView suffix = end.line == line_count() ? StringView{} : m_lines[end.line].substr(end.column);

    auto new_line = (not prefix.empty() or not suffix.empty()) ? StringData::create({prefix, suffix}) : StringDataPtr{};
    m_lines.erase(m_lines.begin() + (int)begin.line, m_lines.begin() + (int)end.line);

    m_changes.push_back({ Change::Erase, begin, end });
    if (new_line)
        m_lines.get_storage(begin.line) = std::move(new_line);

    return begin;
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

BufferRange Buffer::insert(BufferCoord pos, StringView content)
{
    throw_if_read_only();

    kak_assert(is_valid(pos));
    if (content.empty())
        return {pos, pos};

    StringDataPtr real_content;
    if (is_end(pos) and content.back() != '\n')
        real_content = intern(content + "\n");
    else
        real_content = intern(content);

    if (not (m_flags & Flags::NoUndo))
        m_current_undo_group.push_back({Modification::Insert, pos, real_content});
    return do_insert(pos, real_content->strview());
}

BufferCoord Buffer::erase(BufferCoord begin, BufferCoord end)
{
    throw_if_read_only();

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

BufferRange Buffer::replace(BufferCoord begin, BufferCoord end, StringView content)
{
    throw_if_read_only();

    if (std::equal(iterator_at(begin), iterator_at(end), content.begin(), content.end()))
        return {begin, end};

    if (is_end(end) and not content.empty() and content.back() == '\n')
    {
        auto pos = insert(erase(begin, back_coord()),
                          content.substr(0, content.length() - 1)).begin;
        return {pos, end_coord()};
    }
    return insert(erase(begin, end), content);
}

bool Buffer::is_modified() const
{
    return m_flags & Flags::File and
           (m_history_id != m_last_save_history_id or
            not m_current_undo_group.empty());
}

void Buffer::notify_saved(FsStatus status)
{
    if (not m_current_undo_group.empty())
        commit_undo_group();

    m_flags &= ~Flags::New;
    m_last_save_history_id = m_history_id;
    m_fs_status = status;
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
    auto column = (int)(utf8::character_start(line.begin() + (int)coord.column - 1, line.begin()) - line.begin());
    return { coord.line, column };
}

void Buffer::set_fs_status(FsStatus status)
{
    kak_assert(m_flags & Flags::File);
    m_fs_status = std::move(status);
}

const FsStatus& Buffer::fs_status() const
{
    kak_assert(m_flags & Flags::File);
    return m_fs_status;
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
    run_hook_in_own_context(Hook::BufSetOption,
                            format("{}={}", option.name(), option.get_desc_string()));
}

void Buffer::run_hook_in_own_context(Hook hook, StringView param, String client_name)
{
    if (m_flags & Buffer::Flags::NoHooks)
        return;

    InputHandler hook_handler{{ *this, Selection{} },
                              Context::Flags::Draft,
                              std::move(client_name)};
    hooks().run_hook(hook, param, hook_handler.context());
}

Optional<BufferCoord> Buffer::last_modification_coord() const
{
    if (m_history_id == HistoryId::First)
        return {};
    return current_history_node().undo_group.back().coord;
}

String Buffer::debug_description() const
{
    size_t content_size = 0;
    for (auto& line : m_lines)
        content_size += (int)line->strview().length();

    const size_t additional_size = accumulate(m_history, 0, [](size_t s, auto&& history) {
            return sizeof(history) + history.undo_group.size() * sizeof(Modification) + s;
        }) + m_changes.size() * sizeof(Change);

    return format("{}\nFlags: {}{}{}{}{}{}{}{}\nUsed mem: content={} additional={}\n",
                  display_name(),
                  (m_flags & Flags::File) ? "File (" + name() + ") " : "",
                  (m_flags & Flags::New) ? "New " : "",
                  (m_flags & Flags::Fifo) ? "Fifo " : "",
                  (m_flags & Flags::NoUndo) ? "NoUndo " : "",
                  (m_flags & Flags::NoHooks) ? "NoHooks " : "",
                  (m_flags & Flags::Debug) ? "Debug " : "",
                  (m_flags & Flags::ReadOnly) ? "ReadOnly " : "",
                  is_modified() ? "Modified " : "",
                  content_size, additional_size);
}

UnitTest test_buffer{[]()
{
    auto make_lines = [](auto&&... lines) { return BufferLines{StringData::create({lines})...}; };

    Buffer empty_buffer("empty", Buffer::Flags::None, make_lines("\n"));

    Buffer buffer("test", Buffer::Flags::None, make_lines("allo ?\n", "mais que fais la police\n", " hein ?\n", " youpi\n"));
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
    auto make_lines = [](auto&&... lines) { return BufferLines{StringData::create({lines})...}; };

    Buffer buffer("test", Buffer::Flags::None, make_lines("allo ?\n", "mais que fais la police\n", " hein ?\n", " youpi\n"));
    auto pos = buffer.end_coord();
    buffer.insert(pos, "kanaky\n");                           // change 1
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

    kak_assert((int)buffer.line_count() == 3);
    kak_assert(buffer[0_line] == "allo ?\n");
    kak_assert(buffer[1_line] == "mais que fais la police\n");
    kak_assert(buffer[2_line] == "foo\n");

    buffer.move_to((Buffer::HistoryId)3);
    kak_assert((int)buffer.line_count() == 5);
    kak_assert(buffer[0_line] == "allo ?\n");
    kak_assert(buffer[1_line] == "mais que fais la police\n");
    kak_assert(buffer[2_line] == "tchou\n");
    kak_assert(buffer[3_line] == " hein ?\n");
    kak_assert(buffer[4_line] == " youpi\n");

    buffer.move_to((Buffer::HistoryId)4);
    kak_assert((int)buffer.line_count() == 5);
    kak_assert(buffer[0_line] == "allo ?\n");
    kak_assert(buffer[1_line] == "mais que fais la police\n");
    kak_assert(buffer[2_line] == "mutch\n");
    kak_assert(buffer[3_line] == " hein ?\n");
    kak_assert(buffer[4_line] == " youpi\n");

    buffer.move_to(Buffer::HistoryId::First);
    kak_assert((int)buffer.line_count() == 4);
    kak_assert(buffer[0_line] == "allo ?\n");
    kak_assert(buffer[1_line] == "mais que fais la police\n");
    kak_assert(buffer[2_line] == " hein ?\n");
    kak_assert(buffer[3_line] == " youpi\n");
    kak_assert(not buffer.undo());

    buffer.move_to((Buffer::HistoryId)5);
    kak_assert(not buffer.redo());

    buffer.move_to((Buffer::HistoryId)6);
    kak_assert(not buffer.redo());
}};

}
