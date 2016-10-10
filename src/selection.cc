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
    : SelectionList(buffer, std::move(s), buffer.timestamp()) {}

SelectionList::SelectionList(Buffer& buffer, Vector<Selection> s, size_t timestamp)
    : m_buffer(&buffer), m_selections(std::move(s)), m_timestamp(timestamp)
{
    kak_assert(size() > 0);
    m_main = size() - 1;
    check_invariant();
}

SelectionList::SelectionList(Buffer& buffer, Vector<Selection> s)
    : SelectionList(buffer, std::move(s), buffer.timestamp()) {}

namespace
{

BufferCoord update_insert(BufferCoord coord, BufferCoord begin, BufferCoord end)
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
BufferCoord update_erase(BufferCoord coord, BufferCoord begin, BufferCoord end)
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
    const auto lmin = lhs.min(), rmin = rhs.min();
    return lmin == rmin ? lhs.max() < rhs.max() : lmin < rmin;
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
    BufferCoord cur_pos; // last change position at current modification
    BufferCoord old_pos; // last change position at start

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

    BufferCoord get_old_coord(BufferCoord coord) const
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

    BufferCoord get_new_coord(BufferCoord coord) const
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

    BufferCoord get_new_coord_tolerant(BufferCoord coord) const
    {
        if (coord < old_pos)
            return cur_pos;
        return get_new_coord(coord);
    }

    bool relevant(const Buffer::Change& change, BufferCoord old_coord) const
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

void update_forward(ConstArrayView<Buffer::Change> changes, Vector<Selection>& selections)
{
    ForwardChangesTracker changes_tracker;

    auto change_it = changes.begin();
    auto advance_while_relevant = [&](const BufferCoord& pos) mutable {
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
    kak_assert(std::is_sorted(selections.begin(), selections.end(), compare_selections));
}

void update_backward(ConstArrayView<Buffer::Change> changes, Vector<Selection>& selections)
{
    ForwardChangesTracker changes_tracker;

    using ReverseIt = std::reverse_iterator<const Buffer::Change*>;
    auto change_it = ReverseIt(changes.end());
    auto change_end = ReverseIt(changes.begin());
    auto advance_while_relevant = [&](const BufferCoord& pos) mutable {
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
            update_forward({ change_it, forward_end }, ranges);
            ranges.erase(merge_overlapping(ranges.begin(), ranges.end(), dummy, overlaps), ranges.end());
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
            update_backward({ change_it, backward_end }, ranges);
            ranges.erase(merge_overlapping(ranges.begin(), ranges.end(), dummy, overlaps), ranges.end());
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
    }

    const auto end_coord = buffer.end_coord();
    for (auto& range : ranges)
    {
        range.anchor() = std::min(range.anchor(), end_coord);
        range.cursor() = std::min<BufferCoord>(range.cursor(), end_coord);
    }

    auto touches = [&](const Selection& lhs, const Selection& rhs) {
        return buffer.char_next(lhs.max()) >= rhs.min();
    };
    size_t dummy = 0;
    ranges.erase(merge_overlapping(ranges.begin(), ranges.end(), dummy, touches), ranges.end());

    for (auto& sel : ranges)
    {
        kak_assert(buffer.is_valid(sel.anchor()));
        kak_assert(buffer.is_valid(sel.cursor()));

        if (buffer.is_end(sel.anchor()))
            sel.anchor() = buffer.back_coord();
        if (buffer.is_end(sel.cursor()))
            sel.cursor() = buffer.back_coord();

        if (sel.anchor() != sel.cursor())
            sel.cursor() = buffer.char_prev(sel.cursor());
    }
    return ranges;
}

static void clamp(Selection& sel, const Buffer& buffer)
{
    sel.anchor() = buffer.clamp(sel.anchor());
    sel.cursor() = buffer.clamp(sel.cursor());
}

void update_selections(Vector<Selection>& selections, size_t& main, Buffer& buffer, size_t timestamp)
{
    if (timestamp == buffer.timestamp())
        return;

    auto changes = buffer.changes_since(timestamp);
    auto change_it = changes.begin();
    while (change_it != changes.end())
    {
        auto forward_end = forward_sorted_until(change_it, changes.end());
        auto backward_end = backward_sorted_until(change_it, changes.end());

        if (forward_end >= backward_end)
        {
            update_forward({ change_it, forward_end }, selections);
            change_it = forward_end;
        }
        else
        {
            update_backward({ change_it, backward_end }, selections);
            change_it = backward_end;
        }
        selections.erase(
            merge_overlapping(selections.begin(), selections.end(),
                              main, overlaps), selections.end());
        kak_assert(std::is_sorted(selections.begin(), selections.end(),
                                  compare_selections));
    }
    for (auto& sel : selections)
        clamp(sel, buffer);

    selections.erase(merge_overlapping(selections.begin(), selections.end(),
                                       main, overlaps), selections.end());
}

void SelectionList::update()
{
    update_selections(m_selections, m_main, *m_buffer, m_timestamp);
    check_invariant();
    m_timestamp = m_buffer->timestamp();
}

void SelectionList::check_invariant() const
{
#ifdef KAK_DEBUG
    auto& buffer = this->buffer();
    kak_assert(size() > 0);
    kak_assert(m_main < size());
    const size_t timestamp = buffer.timestamp();
    kak_assert(timestamp >= m_timestamp);

    // cannot check further in that case
    if (timestamp != m_timestamp)
        return;

    const auto end_coord = buffer.end_coord();
    BufferCoord last_min{0,0};
    for (auto& sel : m_selections)
    {
        auto& min = sel.min();
        kak_assert(min >= last_min);
        last_min = min;

        const auto anchor = sel.anchor();
        kak_assert(anchor >= BufferCoord{0,0} and anchor < end_coord);
        kak_assert(anchor.column < buffer[anchor.line].length());

        const auto cursor = sel.cursor();
        kak_assert(cursor >= BufferCoord{0,0} and cursor < end_coord);
        kak_assert(cursor.column < buffer[cursor.line].length());
    }
#endif
}

void SelectionList::sort()
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
}

void SelectionList::merge_overlapping()
{
    if (size() == 1)
        return;

    m_selections.erase(Kakoune::merge_overlapping(begin(), end(),
                                                  m_main, overlaps), end());
}

void SelectionList::merge_consecutive()
{
    if (size() == 1)
        return;

    auto touches = [this](const Selection& lhs, const Selection& rhs) {
        return m_buffer->char_next(lhs.max()) >= rhs.min();
    };
    m_selections.erase(Kakoune::merge_overlapping(begin(), end(),
                                                  m_main, touches), end());
}

void SelectionList::sort_and_merge_overlapping()
{
    sort();
    merge_overlapping();
}

static inline void _avoid_eol(const Buffer& buffer, BufferCoord& coord)
{
    auto column = coord.column;
    auto line = buffer[coord.line];
    if (column != 0 and column == line.length() - 1)
        coord.column = line.byte_count_to(line.char_length() - 2);
}

void SelectionList::avoid_eol()
{
    update();
    for (auto& sel : m_selections)
    {
        _avoid_eol(buffer(), sel.anchor());
        _avoid_eol(buffer(), sel.cursor());
    }
}

BufferCoord prepare_insert(Buffer& buffer, const Selection& sel, InsertMode mode)
{
    switch (mode)
    {
    case InsertMode::Insert:
        return sel.min();
    case InsertMode::InsertCursor:
        return sel.cursor();
    case InsertMode::Replace:
        return {}; // replace is handled specially, by calling Buffer::replace
    case InsertMode::Append:
    {
        // special case for end of lines, append to current line instead
        auto pos = sel.max();
        return buffer.byte_at(pos) == '\n' ? pos : buffer.char_next(pos);
    }
    case InsertMode::InsertAtLineBegin:
        return sel.min().line;
    case InsertMode::AppendAtLineEnd:
        return {sel.max().line, buffer[sel.max().line].length() - 1};
    case InsertMode::InsertAtNextLineBegin:
        return sel.max().line+1;
    case InsertMode::OpenLineBelow:
        return buffer.insert(sel.max().line + 1, "\n");
    case InsertMode::OpenLineAbove:
        return buffer.insert(sel.min().line, "\n");
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

        if (mode == InsertMode::Replace)
            pos = replace(*m_buffer, sel, str);
        else
            pos = m_buffer->insert(pos, str);

        auto& change = m_buffer->changes_since(m_timestamp).back();
        changes_tracker.update(*m_buffer, m_timestamp);
        m_timestamp = m_buffer->timestamp();

        if (select_inserted or mode == InsertMode::Replace)
        {
            if (str.empty())
            {
                sel.anchor() = sel.cursor() = m_buffer->clamp(pos);
                continue;
            }

            // we want min and max from *before* we do any change
            auto& min = sel.min();
            auto& max = sel.max();
            min = change.begin;
            max = m_buffer->char_prev(change.end);
        }
        else
        {
            if (str.empty())
                continue;

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
    merge_overlapping();

    ForwardChangesTracker changes_tracker;
    for (auto& sel : m_selections)
    {
        sel.anchor() = changes_tracker.get_new_coord(sel.anchor());
        kak_assert(m_buffer->is_valid(sel.anchor()));
        sel.cursor() = changes_tracker.get_new_coord(sel.cursor());
        kak_assert(m_buffer->is_valid(sel.cursor()));

        auto pos = Kakoune::erase(*m_buffer, sel);
        sel.anchor() = sel.cursor() = m_buffer->clamp(pos);
        changes_tracker.update(*m_buffer, m_timestamp);
    }

    BufferCoord back_coord = m_buffer->back_coord();
    for (auto& sel : m_selections)
    {
        if (sel.anchor() > back_coord)
            sel.anchor() = back_coord;
        if (sel.cursor() > back_coord)
            sel.cursor() = back_coord;
    }

    m_buffer->check_invariant();
}

String selection_to_string(const Selection& selection)
{
    auto& cursor = selection.cursor();
    auto& anchor = selection.anchor();
    return format("{}.{},{}.{}", anchor.line + 1, anchor.column + 1,
                  cursor.line + 1, cursor.column + 1);
}

String selection_list_to_string(const SelectionList& selections)
{
    return join(selections | transform(selection_to_string), ':', false);
}

Selection selection_from_string(StringView desc)
{
    auto comma = find(desc, ',');
    auto dot_anchor = find(StringView{desc.begin(), comma}, '.');
    auto dot_cursor = find(StringView{comma, desc.end()}, '.');

    if (comma == desc.end() or dot_anchor == comma or dot_cursor == desc.end())
        throw runtime_error(format("'{}' does not follow <line>.<column>,<line>.<column> format", desc));

    BufferCoord anchor{str_to_int({desc.begin(), dot_anchor}) - 1,
                     str_to_int({dot_anchor+1, comma}) - 1};

    BufferCoord cursor{str_to_int({comma+1, dot_cursor}) - 1,
                     str_to_int({dot_cursor+1, desc.end()}) - 1};

    return Selection{anchor, cursor};
}

SelectionList selection_list_from_string(Buffer& buffer, StringView desc)
{
    if (desc.empty())
        throw runtime_error{"empty selection description"};

    Vector<Selection> sels;
    for (auto sel_desc : desc | split<StringView>(':'))
    {
        auto sel = selection_from_string(sel_desc);
        clamp(sel, buffer);
        sels.push_back(sel);
    }
    size_t main = 0;
    std::sort(sels.begin(), sels.end(), compare_selections);
    sels.erase(merge_overlapping(sels.begin(), sels.end(), main, overlaps), sels.end());

    return {buffer, std::move(sels)};
}

}
