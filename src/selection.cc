#include "selection.hh"

#include "utf8.hh"
#include "buffer_utils.hh"

namespace Kakoune
{

void Selection::merge_with(const Selection& range)
{
    m_cursor = range.m_cursor;
    if (m_anchor < m_cursor)
        m_anchor = std::min(m_anchor, range.m_anchor);
    if (m_anchor > m_cursor)
        m_anchor = std::max(m_anchor, range.m_anchor);
}

SelectionList::SelectionList(Buffer& buffer, Selection s, size_t timestamp)
    : m_buffer(&buffer), m_selections({ s }), m_timestamp(timestamp)
{
    check_invariant();
}

SelectionList::SelectionList(Buffer& buffer, Selection s)
    : SelectionList(buffer, s, buffer.timestamp())
{}

SelectionList::SelectionList(Buffer& buffer, std::vector<Selection> s, size_t timestamp)
    : m_buffer(&buffer), m_selections(std::move(s)), m_timestamp(timestamp)
{
    kak_assert(size() > 0);
    check_invariant();
}

SelectionList::SelectionList(Buffer& buffer, std::vector<Selection> s)
    : SelectionList(buffer, std::move(s), buffer.timestamp())
{}

namespace
{

ByteCoord update_insert(ByteCoord coord, ByteCoord begin, ByteCoord end)
{
    if (coord < begin)
        return coord;
    if (begin.line == coord.line)
        coord.column += end.column - begin.column;
    coord.line += end.line - begin.line;
    kak_assert(coord.line >= 0 and coord.column >= 0);
    return coord;
}

ByteCoord update_erase(ByteCoord coord, ByteCoord begin, ByteCoord end)
{
    if (coord < begin)
        return coord;
    if (coord <= end)
        return begin;
    if (end.line == coord.line)
        coord.column -= end.column - begin.column;
    coord.line -= end.line - begin.line;
    kak_assert(coord.line >= 0 and coord.column >= 0);
    return coord;
}

ByteCoord update_pos(ByteCoord coord, const Buffer::Change& change)
{
    if (change.type == Buffer::Change::Insert)
        return update_insert(coord, change.begin, change.end);
    else
        return update_erase(coord, change.begin, change.end);
}

// This tracks position changes for changes that are done
// in a forward way (each change takes place at a position)
// *after* the previous one.
struct PositionChangesTracker
{
    ByteCoord last_pos;
    ByteCoord pos_change;

    void update(const Buffer::Change& change)
    {
        if (change.type == Buffer::Change::Insert)
        {
            pos_change.line += change.end.line - change.begin.line;
            if (change.begin.line != last_pos.line)
                pos_change.column = 0;
            pos_change.column += change.end.column - change.begin.column;
            last_pos = change.end;
        }
        else if (change.type == Buffer::Change::Erase)
        {
            pos_change.line -= change.end.line - change.begin.line;
            if (last_pos.line != change.end.line)
                pos_change.column = 0;
            pos_change.column -= change.end.column - change.begin.column;
            last_pos = change.begin;
        }
    }

    void update(const Buffer& buffer, size_t& timestamp)
    {
        for (auto& change : buffer.changes_since(timestamp))
            update(change);
        timestamp = buffer.timestamp();
    }

    ByteCoord get_new_coord(ByteCoord coord)
    {
        if (last_pos.line - pos_change.line == coord.line)
            coord.column += pos_change.column;
        coord.line += pos_change.line;
        return coord;
    }

    void update_sel(Selection& sel)
    {
        sel.anchor() = get_new_coord(sel.anchor());
        sel.cursor() = get_new_coord(sel.cursor());
    }
};

bool relevant(const Buffer::Change& change, ByteCoord coord)
{
    return change.type == Buffer::Change::Insert ? change.begin <= coord
                                                 : change.begin < coord;
}

void update_forward(memoryview<Buffer::Change> changes, std::vector<Selection>& selections)
{
    PositionChangesTracker changes_tracker;

    auto change_it = changes.begin();
    auto advance_while_relevant = [&](const ByteCoord& pos) mutable {
        while (relevant(*change_it, changes_tracker.get_new_coord(pos)) and
               change_it != changes.end())
            changes_tracker.update(*change_it++);
    };

    for (auto& sel : selections)
    {
        auto& sel_min = sel.min();
        auto& sel_max = sel.max();
        advance_while_relevant(sel_min);
        sel_min = changes_tracker.get_new_coord(sel_min);

        advance_while_relevant(sel_max);
        sel_max = changes_tracker.get_new_coord(sel_max);
    }
}

void update_backward(memoryview<Buffer::Change> changes, std::vector<Selection>& selections)
{
    PositionChangesTracker changes_tracker;

    using ReverseIt = std::reverse_iterator<const Buffer::Change*>;
    auto change_it = ReverseIt(changes.end());
    auto change_end = ReverseIt(changes.begin());
    auto advance_while_relevant = [&](const ByteCoord& pos) mutable {
        while (change_it != change_end)
        {
            auto change = *change_it;
            change.begin = changes_tracker.get_new_coord(change.begin);
            change.end = changes_tracker.get_new_coord(change.end);
            if (not relevant(change, changes_tracker.get_new_coord(pos)))
                break;
            changes_tracker.update(change);
            ++change_it;
        }
    };

    for (auto& sel : selections)
    {
        auto& sel_min = sel.min();
        auto& sel_max = sel.max();
        advance_while_relevant(sel_min);
        sel_min = changes_tracker.get_new_coord(sel_min);

        advance_while_relevant(sel_max);
        sel_max = changes_tracker.get_new_coord(sel_max);
    }
}

}

void SelectionList::update()
{
    if (m_timestamp == m_buffer->timestamp())
        return;

    auto forward = [](const Buffer::Change& lhs, const Buffer::Change& rhs)
        { return lhs.begin < rhs.begin; };
    auto backward = [](const Buffer::Change& lhs, const Buffer::Change& rhs)
        { return lhs.begin > rhs.end; };

    auto changes = m_buffer->changes_since(m_timestamp);
    auto change_it = changes.begin();
    while (change_it != changes.end())
    {
        auto forward_end = std::is_sorted_until(change_it, changes.end(), forward);
        auto backward_end = std::is_sorted_until(change_it, changes.end(), backward);

        if (forward_end >= backward_end)
        {
            update_forward({ change_it, forward_end }, m_selections);
            change_it = forward_end;
        }
        else
        {
            update_backward({ change_it, backward_end }, m_selections);
            change_it = backward_end;
        }
        kak_assert(std::is_sorted(m_selections.begin(), m_selections.end(),
                                  compare_selections));
    }
    for (auto& sel : m_selections)
    {
        sel.anchor() = m_buffer->clamp(sel.anchor());
        sel.cursor() = m_buffer->clamp(sel.cursor());
    }
    m_selections.erase(merge_overlapping(begin(), end(), m_main, overlaps), end());
    check_invariant();

    m_timestamp = m_buffer->timestamp();
}

void SelectionList::check_invariant() const
{
#ifdef KAK_DEBUG
    auto& buffer = this->buffer();
    kak_assert(size() > 0);
    kak_assert(m_main < size());
    for (size_t i = 0; i < size(); ++i)
    {
        auto& sel = (*this)[i];
        if (i+1 < size())
            kak_assert((*this)[i].min() <= (*this)[i+1].min());
        kak_assert(buffer.is_valid(sel.anchor()));
        kak_assert(buffer.is_valid(sel.cursor()));
        kak_assert(not buffer.is_end(sel.anchor()));
        kak_assert(not buffer.is_end(sel.cursor()));
        kak_assert(utf8::is_character_start(buffer.byte_at(sel.anchor())));
        kak_assert(utf8::is_character_start(buffer.byte_at(sel.cursor())));
    }
#endif
}

void SelectionList::sort_and_merge_overlapping()
{
    if (size() == 1)
        return;

    const auto& main = this->main();
    const auto main_begin = main.min();
    m_main = std::count_if(begin(), end(), [&](const Selection& sel) {
                               auto begin = sel.min();
                               if (begin == main_begin)
                                   return &sel < &main;
                               else
                                   return begin < main_begin;
                           });
    std::stable_sort(begin(), end(), compare_selections);
    m_selections.erase(merge_overlapping(begin(), end(), m_main, overlaps), end());
}
namespace
{

inline void _avoid_eol(const Buffer& buffer, ByteCoord& coord)
{
    const auto column = coord.column;
    const auto& line = buffer[coord.line];
    if (column != 0 and column == line.length() - 1)
        coord.column = line.byte_count_to(line.char_length() - 2);
}


inline void _avoid_eol(const Buffer& buffer, Selection& sel)
{
    _avoid_eol(buffer, sel.anchor());
    _avoid_eol(buffer, sel.cursor());
}

}

void SelectionList::avoid_eol()
{
    update();
    for (auto& sel : m_selections)
        _avoid_eol(buffer(), sel);
}

BufferIterator prepare_insert(Buffer& buffer, const Selection& sel, InsertMode mode)
{
    switch (mode)
    {
    case InsertMode::Insert:
        return buffer.iterator_at(sel.min());
    case InsertMode::InsertCursor:
        return buffer.iterator_at(sel.cursor());
    case InsertMode::Replace:
        return erase(buffer, sel);
    case InsertMode::Append:
    {
        // special case for end of lines, append to current line instead
        auto pos = buffer.iterator_at(sel.max());
        return *pos == '\n' ? pos : utf8::next(pos);
    }
    case InsertMode::InsertAtLineBegin:
        return buffer.iterator_at(sel.min().line);
    case InsertMode::AppendAtLineEnd:
        return buffer.iterator_at({sel.max().line, buffer[sel.max().line].length() - 1});
    case InsertMode::InsertAtNextLineBegin:
        return buffer.iterator_at(sel.max().line+1);
    case InsertMode::OpenLineBelow:
        return buffer.insert(buffer.iterator_at(sel.max().line + 1), "\n");
    case InsertMode::OpenLineAbove:
        return buffer.insert(buffer.iterator_at(sel.min().line), "\n");
    }
    kak_assert(false);
    return {};
}

void SelectionList::insert(memoryview<String> strings, InsertMode mode)
{
    if (strings.empty())
        return;

    update();
    PositionChangesTracker changes_tracker;
    for (size_t index = 0; index < m_selections.size(); ++index)
    {
        auto& sel = m_selections[index];

        changes_tracker.update_sel(sel);
        kak_assert(m_buffer->is_valid(sel.anchor()));
        kak_assert(m_buffer->is_valid(sel.cursor()));

        auto pos = prepare_insert(*m_buffer, sel, mode);
        changes_tracker.update(*m_buffer, m_timestamp);

        const String& str = strings[std::min(index, strings.size()-1)];
        pos = m_buffer->insert(pos, str);

        auto& change = m_buffer->changes_since(m_timestamp).back();
        changes_tracker.update(change);
        m_timestamp = m_buffer->timestamp();

        if (mode == InsertMode::Replace)
        {
            sel.anchor() = change.begin;
            sel.cursor() = m_buffer->char_prev(change.end);
        }
        else
        {
            sel.anchor() = m_buffer->clamp(update_insert(sel.anchor(), change.begin, change.end));
            sel.cursor() = m_buffer->clamp(update_insert(sel.cursor(), change.begin, change.end));
        }
    }
    check_invariant();
    m_buffer->check_invariant();
}

void SelectionList::erase()
{
    update();
    PositionChangesTracker changes_tracker;
    for (auto& sel : m_selections)
    {
        changes_tracker.update_sel(sel);
        auto pos = Kakoune::erase(*m_buffer, sel);
        sel.anchor() = sel.cursor() = m_buffer->clamp(pos.coord());
        changes_tracker.update(*m_buffer, m_timestamp);
    }
    m_buffer->check_invariant();
}

void update_insert(std::vector<Selection>& sels, ByteCoord begin, ByteCoord end)
{
    for (auto& sel : sels)
    {
        sel.anchor() = update_insert(sel.anchor(), begin, end);
        sel.cursor() = update_insert(sel.cursor(), begin, end);
    }
}

void update_erase(std::vector<Selection>& sels, ByteCoord begin, ByteCoord end)
{
    for (auto& sel : sels)
    {
        sel.anchor() = update_erase(sel.anchor(), begin, end);
        sel.cursor() = update_erase(sel.cursor(), begin, end);
    }
}

}
