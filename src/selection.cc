#include "selection.hh"

#include "buffer_utils.hh"
#include "changes.hh"
#include "utf8.hh"

namespace Kakoune
{

SelectionList::SelectionList(Buffer& buffer, Selection s, size_t timestamp)
    : m_buffer(&buffer), m_selections({ std::move(s) }), m_timestamp(timestamp)
{
    check_invariant();
}

SelectionList::SelectionList(Buffer& buffer, Selection s)
    : SelectionList(buffer, std::move(s), buffer.timestamp()) {}

SelectionList::SelectionList(Buffer& buffer, Vector<Selection> list, size_t timestamp)
    : m_buffer(&buffer), m_selections(std::move(list)), m_timestamp(timestamp)
{
    kak_assert(size() > 0);
    m_main = size() - 1;
    check_invariant();
}

SelectionList::SelectionList(Buffer& buffer, Vector<Selection> list)
    : SelectionList(buffer, std::move(list), buffer.timestamp()) {}

SelectionList::SelectionList(SelectionList::UnsortedTag, Buffer& buffer, Vector<Selection> list, size_t timestamp, size_t main)
    : m_buffer(&buffer), m_selections(std::move(list)), m_timestamp(timestamp)
{
    sort_and_merge_overlapping();
    check_invariant();
}

void SelectionList::remove(size_t index)
{
    m_selections.erase(begin() + index);
    if (index < m_main or m_main == m_selections.size())
        --m_main;
}
void SelectionList::set(Vector<Selection> list, size_t main)
{
    kak_assert(main < list.size());
    m_selections = std::move(list);
    m_main = main;
    m_timestamp = m_buffer->timestamp();
    sort_and_merge_overlapping();
    check_invariant();
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

Vector<Selection> compute_modified_ranges(Buffer& buffer, size_t timestamp)
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
        kak_assert(std::is_sorted(selections.begin(), selections.end(),
                                  compare_selections));
        selections.erase(
            merge_overlapping(selections.begin(), selections.end(),
                              main, overlaps), selections.end());
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

BufferCoord get_insert_pos(const Buffer& buffer, const Selection& sel,
                           InsertMode mode)
{
    switch (mode)
    {
    case InsertMode::Insert:
        return sel.min();
    case InsertMode::InsertCursor:
        return sel.cursor();
    case InsertMode::Append:
        return buffer.char_next(sel.max());
    case InsertMode::InsertAtLineBegin:
        return sel.min().line;
    case InsertMode::AppendAtLineEnd:
        return {sel.max().line, buffer[sel.max().line].length() - 1};
    case InsertMode::InsertAtNextLineBegin:
        return sel.max().line+1;
    default:
        kak_assert(false);
        return {};
    }
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

void SelectionList::insert(ConstArrayView<String> strings, InsertMode mode,
                           Vector<BufferCoord>* out_insert_pos)
{
    if (strings.empty())
        return;

    update();

    Vector<BufferCoord> insert_pos;
    if (mode != InsertMode::Replace)
    {
        for (auto& sel : m_selections)
            insert_pos.push_back(get_insert_pos(*m_buffer, sel, mode));
    }

    ForwardChangesTracker changes_tracker;
    for (size_t index = 0; index < m_selections.size(); ++index)
    {
        auto& sel = m_selections[index];

        sel.anchor() = changes_tracker.get_new_coord_tolerant(sel.anchor());
        sel.cursor() = changes_tracker.get_new_coord_tolerant(sel.cursor());
        kak_assert(m_buffer->is_valid(sel.anchor()) and
                   m_buffer->is_valid(sel.cursor()));

        const String& str = strings[std::min(index, strings.size()-1)];

        const auto pos = (mode == InsertMode::Replace) ?
            replace(*m_buffer, sel, str)
          : m_buffer->insert(changes_tracker.get_new_coord(insert_pos[index]), str);

        size_t old_timestamp = m_timestamp;
        changes_tracker.update(*m_buffer, m_timestamp);

        if (out_insert_pos)
            out_insert_pos->push_back(pos);

        if (mode == InsertMode::Replace)
        {
            auto changes = m_buffer->changes_since(old_timestamp);
            if (changes.size() == 1) // Nothing got inserted, either str was empty, or just \n at end of buffer
                sel.anchor() = sel.cursor() = m_buffer->clamp(pos);
            else if (changes.size() == 2)
            {
                // we want min and max from *before* we do any change
                auto& min = sel.min();
                auto& max = sel.max();
                min = changes.back().begin;
                max = m_buffer->char_prev(changes.back().end);
            }
            else
                kak_assert(changes.empty());
        }
        else if (not str.empty())
        {
            auto& change = m_buffer->changes_since(0).back();
            sel.anchor() = m_buffer->clamp(update_insert(sel.anchor(), change.begin, change.end));
            sel.cursor() = m_buffer->clamp(update_insert(sel.cursor(), change.begin, change.end));
        }
    }

    // We might just have been deleting text if strings were empty,
    // in which case we could have some selections pushed out of the buffer
    if (mode == InsertMode::Replace)
        fix_overflowing_selections(m_selections, *m_buffer);

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
        sel.anchor() = sel.cursor() = pos;
        changes_tracker.update(*m_buffer, m_timestamp);
    }

    fix_overflowing_selections(m_selections, *m_buffer);
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
    auto beg = &*selections.begin(), end = &*selections.end();
    auto main = beg + selections.main_index();
    using View = ConstArrayView<Selection>;
    return join(concatenated(View{main, end}, View{beg, main}) |
                transform(selection_to_string), ' ', false);
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

    if (anchor.line < 0 or anchor.column < 0 or
        cursor.line < 0 or cursor.column < 0)
        throw runtime_error(format("coordinates must be >= 1: '{}'", desc));

    return Selection{anchor, cursor};
}

SelectionList selection_list_from_string(Buffer& buffer, ConstArrayView<String> descs)
{
    if (descs.empty())
        throw runtime_error{"empty selection description"};

    auto sels = descs | transform([&](auto&& d) { auto s = selection_from_string(d); clamp(s, buffer); return s; })
                      | gather<Vector<Selection>>();
    return {SelectionList::UnsortedTag{}, buffer, std::move(sels), buffer.timestamp(), 0};
}

}
