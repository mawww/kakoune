#include "selection.hh"

#include "buffer_utils.hh"
#include "changes.hh"
#include "utf8.hh"

namespace Kakoune
{

SelectionList::SelectionList(Buffer& buffer, Selection s, size_t timestamp)
    : m_selections({ std::move(s) }), m_buffer(&buffer), m_timestamp(timestamp)
{
    check_invariant();
}

SelectionList::SelectionList(Buffer& buffer, Selection s)
    : SelectionList(buffer, std::move(s), buffer.timestamp()) {}

SelectionList::SelectionList(Buffer& buffer, Vector<Selection> list, size_t timestamp)
    : m_selections(std::move(list)), m_buffer(&buffer), m_timestamp(timestamp)
{
    kak_assert(size() > 0);
    m_main = size() - 1;
    check_invariant();
}

SelectionList::SelectionList(Buffer& buffer, Vector<Selection> list)
    : SelectionList(buffer, std::move(list), buffer.timestamp()) {}

void SelectionList::remove(size_t index)
{
    m_selections.erase(begin() + index);
    if (index < m_main or m_main == m_selections.size())
        --m_main;
}

void SelectionList::remove_from(size_t index)
{
    kak_assert(index > 0);
    m_selections.erase(begin() + index, end());
    if (index <= m_main)
        m_main = m_selections.size() - 1;
}

void SelectionList::set(Vector<Selection> list, size_t main)
{
    kak_assert(main < list.size());
    m_selections = std::move(list);
    m_main = main;
    m_timestamp = m_buffer->timestamp();
    sort();
    check_invariant();
}

bool compare_selections(const Selection& lhs, const Selection& rhs)
{
    const auto& lmin = lhs.min(), rmin = rhs.min();
    return lmin == rmin ? lhs.max() < rhs.max() : lmin < rmin;
}

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
            begin[i].min() = std::min(begin[i].min(), begin[j].min());
            begin[i].max() = std::max(begin[i].max(), begin[j].max());
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
    kak_assert(std::is_sorted(begin, begin + i +1, compare_selections));
    return begin + i + 1;
}

}

BufferCoord& get_first(Selection& sel) { return sel.min(); }
BufferCoord& get_last(Selection& sel) { return sel.max(); }

Vector<Selection> compute_modified_ranges(const Buffer& buffer, size_t timestamp)
{
    Vector<Selection> ranges;
    auto changes = buffer.changes_since(timestamp);
    auto change_it = changes.begin();
    while (change_it != changes.end())
    {
        auto forward_end = forward_sorted_until(change_it, changes.end());
        auto backward_end = backward_sorted_until(change_it, changes.end());

        kak_assert(std::is_sorted(ranges.begin(), ranges.end(), compare_selections));

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
                    ranges.emplace_back(change_it->begin, change_it->end);
                else
                    ranges.emplace_back(change_it->begin);
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
                    ranges.emplace_back(change.begin, change.end);
                else
                    ranges.emplace_back(change.begin);
                changes_tracker.update(change);
            }
            change_it = backward_end;
        }

        kak_assert(std::is_sorted(ranges.begin() + prev_size, ranges.end(), compare_selections));
        std::inplace_merge(ranges.begin(), ranges.begin() + prev_size, ranges.end(), compare_selections);
        // The newly added ranges might be overlapping pre-existing ones
        ranges.erase(merge_overlapping(ranges.begin(), ranges.end(), dummy, overlaps), ranges.end());
    }

    const auto end_coord = buffer.end_coord();
    for (auto& range : ranges)
    {
        range.anchor() = std::min(range.anchor(), end_coord);
        range.cursor() = std::min<BufferCoord>(range.cursor(), end_coord);
    }

    auto touches = [&](const Selection& lhs, const Selection& rhs) {
        return lhs.max() == end_coord or buffer.char_next(lhs.max()) >= rhs.min();
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

void clamp_selections(Vector<Selection>& selections, const Buffer& buffer)
{
    for (auto& sel : selections)
        clamp(sel, buffer);
}

void update_selections(Vector<Selection>& selections, size_t& main, const Buffer& buffer, size_t timestamp, bool merge)
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
        kak_assert(std::is_sorted(selections.begin(), selections.end(),
                                  compare_selections));
        if (merge)
            selections.erase(
                merge_overlapping(selections.begin(), selections.end(),
                                  main, overlaps), selections.end());
    }
    for (auto& sel : selections)
        clamp(sel, buffer);

    if (merge)
        selections.erase(merge_overlapping(selections.begin(), selections.end(),
                                           main, overlaps), selections.end());
}

void SelectionList::update(bool merge)
{
    update_selections(m_selections, m_main, *m_buffer, m_timestamp, merge);
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

void sort_selections(Vector<Selection>& selections, size_t& main_index)
{
    if (selections.size() == 1)
        return;

    const auto& main = selections[main_index];
    const auto main_begin = main.min();
    main_index = std::count_if(selections.begin(), selections.end(),
                               [&](const Selection& sel) {
        auto begin = sel.min();
        if (begin == main_begin)
            return &sel < &main;
        else
            return begin < main_begin;
    });
    std::stable_sort(selections.begin(), selections.end(), compare_selections);
}

void merge_overlapping_selections(Vector<Selection>& selections, size_t& main_index)
{
    if (selections.size() == 1)
        return;

    selections.erase(Kakoune::merge_overlapping(selections.begin(), selections.end(),
                                                main_index, overlaps), selections.end());
}

void SelectionList::sort()
{
    sort_selections(m_selections, m_main);
}

void SelectionList::merge_overlapping()
{
    merge_overlapping_selections(m_selections, m_main);
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

static void fix_overflowing_selections(Vector<Selection>& selections,
                                       const Buffer& buffer)
{
    const BufferCoord back_coord = buffer.back_coord();
    for (auto& sel : selections)
    {
        sel.cursor() = std::min(buffer.clamp(sel.cursor()), back_coord);
        sel.anchor() = std::min(buffer.clamp(sel.anchor()), back_coord);
    }
}

void SelectionList::for_each(ApplyFunc func)
{
    update();

    ForwardChangesTracker changes_tracker;
    for (size_t index = 0; index < m_selections.size(); ++index)
    {
        auto& sel = m_selections[index];

        sel.anchor() = changes_tracker.get_new_coord_tolerant(sel.anchor());
        sel.cursor() = changes_tracker.get_new_coord_tolerant(sel.cursor());
        kak_assert(m_buffer->is_valid(sel.anchor()) and m_buffer->is_valid(sel.cursor()));

        func(index, sel);

        changes_tracker.update(*m_buffer, m_timestamp);
    }

    // We might just have been deleting text if strings were empty,
    // in which case we could have some selections pushed out of the buffer
    fix_overflowing_selections(m_selections, *m_buffer);

    check_invariant();
    m_buffer->check_invariant();
}


void replace(Buffer& buffer, Selection& sel, StringView content)
{
    // we want min and max from *before* we do any change
    auto& min = sel.min();
    auto& max = sel.max();
    BufferRange range = buffer.replace(min, buffer.char_next(max), content);
    min = range.begin;
    max = range.end > range.begin ? buffer.char_prev(range.end) : range.begin;
}

BufferRange insert(Buffer& buffer, Selection& sel, BufferCoord pos, StringView content)
{
    auto range = buffer.insert(pos, content);
    sel.anchor() = buffer.clamp(update_insert(sel.anchor(), range.begin, range.end));
    sel.cursor() = buffer.clamp(update_insert(sel.cursor(), range.begin, range.end));
    return range;
}

void SelectionList::replace(ConstArrayView<String> strings)
{
    if (strings.empty())
        return;

    for_each([&](size_t index, Selection& sel) {
        Kakoune::replace(*m_buffer, sel, strings[std::min(strings.size()-1, index)]);
    });
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
        sel.anchor() = sel.cursor() = pos;
        changes_tracker.update(*m_buffer, m_timestamp);
    }

    fix_overflowing_selections(m_selections, *m_buffer);
    m_buffer->check_invariant();
}

String selection_to_string(ColumnType column_type, const Buffer& buffer, const Selection& selection, ColumnCount tabstop)
{
    const auto& cursor = selection.cursor();
    const auto& anchor = selection.anchor();
    switch (column_type)
    {
    default:
    case ColumnType::Byte:
        return format("{}.{},{}.{}", anchor.line + 1, anchor.column + 1,
                      cursor.line + 1, cursor.column + 1);
    case ColumnType::Codepoint:
        return format("{}.{},{}.{}",
                      anchor.line + 1, buffer[anchor.line].char_count_to(anchor.column) + 1,
                      cursor.line + 1, buffer[cursor.line].char_count_to(cursor.column) + 1);
    case ColumnType::DisplayColumn:
        kak_assert(tabstop != -1);
        return format("{}.{},{}.{}",
                      anchor.line + 1, get_column(buffer, tabstop, anchor) + 1,
                      cursor.line + 1, get_column(buffer, tabstop, cursor) + 1);
    }
}

String selection_list_to_string(ColumnType column_type, const SelectionList& selections, ColumnCount tabstop)
{
    auto& buffer = selections.buffer();
    kak_assert(selections.timestamp() == buffer.timestamp());

    auto to_string = [&](const Selection& selection) {
        return selection_to_string(column_type, buffer, selection, tabstop);
    };

    auto beg = &*selections.begin(), end = &*selections.end();
    auto main = beg + selections.main_index();
    using View = ConstArrayView<Selection>;
    return join(concatenated(View{main, end}, View{beg, main}) |
                transform(to_string), ' ', false);
}

Selection selection_from_string(ColumnType column_type, const Buffer& buffer, StringView desc, ColumnCount tabstop)
{
    auto comma = find(desc, ',');
    auto dot_anchor = find(StringView{desc.begin(), comma}, '.');
    auto dot_cursor = find(StringView{comma, desc.end()}, '.');

    if (comma == desc.end() or dot_anchor == comma or dot_cursor == desc.end())
        throw runtime_error(format("'{}' does not follow <line>.<column>,<line>.<column> format", desc));

    auto compute_coord = [&](int line, int column) -> BufferCoord {
        if (line < 0 or column < 0)
            throw runtime_error(format("coordinate {}.{} does not exist in buffer", line + 1, column + 1));

        switch (column_type)
        {
        default:
        case ColumnType::Byte: return {line, column};
        case ColumnType::Codepoint:
            if (buffer.line_count() <= line or buffer[line].char_length() <= column)
                throw runtime_error(format("coordinate {}.{} does not exist in buffer", line + 1, column + 1));
            return {line, buffer[line].byte_count_to(CharCount{column})};
        case ColumnType::DisplayColumn:
            kak_assert(tabstop != -1);
            if (buffer.line_count() <= line or column_length(buffer, tabstop, line) <= column)
                throw runtime_error(format("coordinate {}.{} does not exist in buffer", line + 1, column + 1));
            return {line, get_byte_to_column(buffer, tabstop, DisplayCoord{line, ColumnCount{column}})};
        }
    };

    auto anchor = compute_coord(str_to_int({desc.begin(), dot_anchor}) - 1,
                                str_to_int({dot_anchor+1, comma}) - 1);

    auto cursor = compute_coord(str_to_int({comma+1, dot_cursor}) - 1,
                                str_to_int({dot_cursor+1, desc.end()}) - 1);

    return Selection{anchor, cursor};
}

}
