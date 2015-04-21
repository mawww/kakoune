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
    : m_buffer(&buffer), m_selections({ std::move(s) }), m_timestamp(timestamp)
{
    check_invariant();
}

SelectionList::SelectionList(Buffer& buffer, Selection s)
    : SelectionList(buffer, std::move(s), buffer.timestamp())
{}

SelectionList::SelectionList(Buffer& buffer, Vector<Selection> s, size_t timestamp)
    : m_buffer(&buffer), m_selections(std::move(s)), m_timestamp(timestamp)
{
    kak_assert(size() > 0);
    check_invariant();
}

SelectionList::SelectionList(Buffer& buffer, Vector<Selection> s)
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

/* For reference
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
} */

bool compare_selections(const Selection& lhs, const Selection& rhs)
{
    return lhs.min() < rhs.min();
}

template<typename Iterator, typename OverlapsFunc>
Iterator merge_overlapping(Iterator begin, Iterator end, size_t& main, OverlapsFunc overlaps)
{
    if (begin == end)
        return begin;

    kak_assert(std::is_sorted(begin, end, compare_selections));
    size_t size = end - begin;
    size_t i = 0;
    for (size_t j = 1; j < size; ++j)
    {
        if (overlaps(begin[i], begin[j]))
        {
            begin[i].merge_with(begin[j]);
            if (i < main)
                --main;
        }
        else
        {
            ++i;
            if (i != j)
                begin[i] = std::move(begin[j]);
        }
    }
    return begin + i + 1;
}

// This tracks position changes for changes that are done
// in a forward way (each change takes place at a position)
// *after* the previous one.
struct ForwardChangesTracker
{
    ByteCoord cur_pos; // last change position at current modification
    ByteCoord old_pos; // last change position at start

    void update(const Buffer::Change& change)
    {
        kak_assert(change.begin >= cur_pos);

        if (change.type == Buffer::Change::Insert)
        {
            old_pos = get_old_coord(change.begin);
            cur_pos = change.end;
        }
        else if (change.type == Buffer::Change::Erase)
        {
            old_pos = get_old_coord(change.end);
            cur_pos = change.begin;
        }
    }

    void update(const Buffer& buffer, size_t& timestamp)
    {
        for (auto& change : buffer.changes_since(timestamp))
            update(change);
        timestamp = buffer.timestamp();
    }

    ByteCoord get_old_coord(ByteCoord coord) const
    {
        kak_assert(cur_pos <= coord);
        auto pos_change = cur_pos - old_pos;
        if (cur_pos.line == coord.line)
        {
            kak_assert(pos_change.column <= coord.column);
            coord.column -= pos_change.column;
        }
        coord.line -= pos_change.line;
        kak_assert(old_pos <= coord);
        return coord;
    }

    ByteCoord get_new_coord(ByteCoord coord) const
    {
        kak_assert(old_pos <= coord);
        auto pos_change = cur_pos - old_pos;
        if (old_pos.line == coord.line)
        {
            kak_assert(-pos_change.column <= coord.column);
            coord.column += pos_change.column;
        }
        coord.line += pos_change.line;
        kak_assert(cur_pos <= coord);
        return coord;
    }

    ByteCoord get_new_coord_tolerant(ByteCoord coord) const
    {
        if (coord < old_pos)
            return cur_pos;
        return get_new_coord(coord);
    }

    bool relevant(const Buffer::Change& change, ByteCoord old_coord) const
    {
        auto new_coord = get_new_coord_tolerant(old_coord);
        return change.type == Buffer::Change::Insert ? change.begin <= new_coord
                                                     : change.begin < new_coord;
    }
};

const Buffer::Change* forward_sorted_until(const Buffer::Change* first, const Buffer::Change* last)
{
    if (first != last) {
        const Buffer::Change* next = first;
        while (++next != last) {
            const auto& ref = first->type == Buffer::Change::Insert ? first->end : first->begin;
            if (next->begin <= ref)
                return next;
            first = next;
        }
    }
    return last;
}

const Buffer::Change* backward_sorted_until(const Buffer::Change* first, const Buffer::Change* last)
{
    if (first != last) {
        const Buffer::Change* next = first;
        while (++next != last) {
            if (first->begin <= next->end)
                return next;
            first = next;
        }
    }
    return last;
}

void update_forward(ConstArrayView<Buffer::Change> changes, Vector<Selection>& selections, size_t& main)
{
    ForwardChangesTracker changes_tracker;

    auto change_it = changes.begin();
    auto advance_while_relevant = [&](const ByteCoord& pos) mutable {
        while (change_it != changes.end() and changes_tracker.relevant(*change_it, pos))
            changes_tracker.update(*change_it++);
    };

    for (auto& sel : selections)
    {
        auto& sel_min = sel.min();
        auto& sel_max = sel.max();
        advance_while_relevant(sel_min);
        sel_min = changes_tracker.get_new_coord_tolerant(sel_min);

        advance_while_relevant(sel_max);
        sel_max = changes_tracker.get_new_coord_tolerant(sel_max);
    }
    selections.erase(merge_overlapping(selections.begin(), selections.end(), main, overlaps),
                     selections.end());
    kak_assert(std::is_sorted(selections.begin(), selections.end(), compare_selections));
}

void update_backward(ConstArrayView<Buffer::Change> changes, Vector<Selection>& selections, size_t& main)
{
    ForwardChangesTracker changes_tracker;

    using ReverseIt = std::reverse_iterator<const Buffer::Change*>;
    auto change_it = ReverseIt(changes.end());
    auto change_end = ReverseIt(changes.begin());
    auto advance_while_relevant = [&](const ByteCoord& pos) mutable {
        while (change_it != change_end)
        {
            auto change = *change_it;
            change.begin = changes_tracker.get_new_coord(change.begin);
            change.end = changes_tracker.get_new_coord(change.end);
            if (not changes_tracker.relevant(change, pos))
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
        sel_min = changes_tracker.get_new_coord_tolerant(sel_min);

        advance_while_relevant(sel_max);
        sel_max = changes_tracker.get_new_coord_tolerant(sel_max);
    }
    selections.erase(merge_overlapping(selections.begin(), selections.end(), main, overlaps),
                     selections.end());
    kak_assert(std::is_sorted(selections.begin(), selections.end(), compare_selections));
}

}

Vector<Selection> compute_modified_ranges(Buffer& buffer, size_t timestamp)
{
    Vector<Selection> ranges;
    auto changes = buffer.changes_since(timestamp);
    auto change_it = changes.begin();
    while (change_it != changes.end())
    {
        auto forward_end = forward_sorted_until(change_it, changes.end());
        auto backward_end = backward_sorted_until(change_it, changes.end());

        size_t prev_size;
        size_t dummy = 0;
        if (forward_end >= backward_end)
        {
            update_forward({ change_it, forward_end }, ranges, dummy);
            prev_size = ranges.size();

            ForwardChangesTracker changes_tracker;
            for (; change_it != forward_end; ++change_it)
            {
                if (change_it->type == Buffer::Change::Insert)
                    ranges.push_back({ change_it->begin, change_it->end });
                else
                    ranges.push_back({ change_it->begin });
                changes_tracker.update(*change_it);
            }
        }
        else
        {
            update_backward({ change_it, backward_end }, ranges, dummy);
            prev_size = ranges.size();

            using ReverseIt = std::reverse_iterator<const Buffer::Change*>;
            ForwardChangesTracker changes_tracker;
            for (ReverseIt it{backward_end}, end{change_it}; it != end; ++it)
            {
                auto change = *it;
                change.begin = changes_tracker.get_new_coord(change.begin);
                change.end = changes_tracker.get_new_coord(change.end);

                if (change.type == Buffer::Change::Insert)
                    ranges.push_back({ change.begin, change.end });
                else
                    ranges.push_back({ change.begin });
                changes_tracker.update(change);
            }
            change_it = backward_end;
        }

        kak_assert(std::is_sorted(ranges.begin() + prev_size, ranges.end(), compare_selections));
        std::inplace_merge(ranges.begin(), ranges.begin() + prev_size, ranges.end(), compare_selections);
        ranges.erase(merge_overlapping(ranges.begin(), ranges.end(), dummy, overlaps), ranges.end());
    }
    for (auto& sel : ranges)
    {
        sel.anchor() = buffer.clamp(sel.anchor());
        sel.cursor() = buffer.clamp(sel.cursor());
    }

    auto touches = [&](const Selection& lhs, const Selection& rhs) {
        return buffer.char_next(lhs.max()) >= rhs.min();
    };
    size_t dummy = 0;
    ranges.erase(merge_overlapping(ranges.begin(), ranges.end(), dummy, touches), ranges.end());

    for (auto& sel : ranges)
    {
        if (sel.anchor() != sel.cursor())
            sel.cursor() = buffer.char_prev(sel.cursor());
    }
    return ranges;
}

void SelectionList::update()
{
    if (m_timestamp == m_buffer->timestamp())
        return;

    auto changes = m_buffer->changes_since(m_timestamp);
    auto change_it = changes.begin();
    while (change_it != changes.end())
    {
        auto forward_end = forward_sorted_until(change_it, changes.end());
        auto backward_end = backward_sorted_until(change_it, changes.end());

        if (forward_end >= backward_end)
        {
            update_forward({ change_it, forward_end }, m_selections, m_main);
            change_it = forward_end;
        }
        else
        {
            update_backward({ change_it, backward_end }, m_selections, m_main);
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
    auto column = coord.column;
    auto line = buffer[coord.line];
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
        return *pos == '\n' ? pos : utf8::next(pos, buffer.end());
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

void SelectionList::insert(ConstArrayView<String> strings, InsertMode mode,
                           bool select_inserted)
{
    if (strings.empty())
        return;

    update();
    ForwardChangesTracker changes_tracker;
    for (size_t index = 0; index < m_selections.size(); ++index)
    {
        auto& sel = m_selections[index];

        sel.anchor() = changes_tracker.get_new_coord(sel.anchor());
        kak_assert(m_buffer->is_valid(sel.anchor()));
        sel.cursor() = changes_tracker.get_new_coord(sel.cursor());
        kak_assert(m_buffer->is_valid(sel.cursor()));

        auto pos = prepare_insert(*m_buffer, sel, mode);
        changes_tracker.update(*m_buffer, m_timestamp);

        const String& str = strings[std::min(index, strings.size()-1)];
        if (str.empty())
        {
            if (mode == InsertMode::Replace)
                sel.anchor() = sel.cursor() = m_buffer->clamp(pos.coord());
            continue;
        }

        pos = m_buffer->insert(pos, str);

        auto& change = m_buffer->changes_since(m_timestamp).back();
        changes_tracker.update(change);
        m_timestamp = m_buffer->timestamp();

        if (select_inserted or mode == InsertMode::Replace)
        {
            // we want min and max from *before* we do any change
            auto& min = sel.min();
            auto& max = sel.max();
            min = change.begin;
            max = m_buffer->char_prev(change.end);
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
    ForwardChangesTracker changes_tracker;
    for (auto& sel : m_selections)
    {
        sel.anchor() = changes_tracker.get_new_coord(sel.anchor());
        kak_assert(m_buffer->is_valid(sel.anchor()));
        sel.cursor() = changes_tracker.get_new_coord(sel.cursor());
        kak_assert(m_buffer->is_valid(sel.cursor()));

        auto pos = Kakoune::erase(*m_buffer, sel);
        sel.anchor() = sel.cursor() = m_buffer->clamp(pos.coord());
        changes_tracker.update(*m_buffer, m_timestamp);
    }

    ByteCoord back_coord = m_buffer->back_coord();
    for (auto& sel : m_selections)
    {
        if (sel.anchor() > back_coord)
            sel.anchor() = back_coord;
        if (sel.cursor() > back_coord)
            sel.cursor() = back_coord;
    }
    m_buffer->check_invariant();
}

String selection_to_string(const Buffer& buffer, const Selection& selection)
{
    auto& cursor = selection.cursor();
    auto& anchor = selection.anchor();
    ByteCount distance = buffer.distance(anchor, cursor);
    return format("{}.{}{}{}", anchor.line + 1, anchor.column + 1,
                  distance < 0 ? '-' : '+', abs(distance));
}

String selection_list_to_string(const SelectionList& selections)
{
    const auto& buffer = selections.buffer();
    return join(transformed(selections, [&buffer](const Selection& s)
                            { return selection_to_string(buffer, s); }),
                ':', false);
}

Selection selection_from_string(const Buffer& buffer, StringView desc)
{
    auto dot = find(desc, '.');
    auto sign = std::find_if(dot, desc.end(), [](char c) { return c == '+' or c == '-'; });

    if (dot == desc.end() or sign == desc.end())
        throw runtime_error(format("'{}' does not follow <line>.<column>+<len> format", desc));

    LineCount line = str_to_int({desc.begin(), dot}) - 1;
    ByteCount column = str_to_int({dot+1, sign}) - 1;
    ByteCoord anchor{line, column};
    ByteCount count = (*sign == '+' ? 1 : -1) * str_to_int({sign+1, desc.end()});
    return Selection{anchor, buffer.advance(anchor, count)};
}

SelectionList selection_list_from_string(Buffer& buffer, StringView desc)
{
    Vector<Selection> sels;
    for (auto sel_desc : split(desc, ':'))
        sels.push_back(selection_from_string(buffer, sel_desc));
    return {buffer, std::move(sels)};
}

}
