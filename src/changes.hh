#ifndef changes_hh_INCLUDED
#define changes_hh_INCLUDED

#include "buffer.hh"
#include "coord.hh"

namespace Kakoune
{

// This tracks position changes for changes that are done
// in a forward way (each change takes place at a position)
// *after* the previous one.
struct ForwardChangesTracker
{
    BufferCoord cur_pos; // last change position at current modification
    BufferCoord old_pos; // last change position at start

    void update(const Buffer::Change& change);
    void update(const Buffer& buffer, size_t& timestamp);

    BufferCoord get_old_coord(BufferCoord coord) const;
    BufferCoord get_new_coord(BufferCoord coord) const;
    BufferCoord get_new_coord_tolerant(BufferCoord coord) const;

    bool relevant(const Buffer::Change& change, BufferCoord old_coord) const;
};

const Buffer::Change* forward_sorted_until(const Buffer::Change* first, const Buffer::Change* last);
const Buffer::Change* backward_sorted_until(const Buffer::Change* first, const Buffer::Change* last);

template<typename Range, typename AdvanceFunc>
auto update_range(ForwardChangesTracker& changes_tracker, Range& range, AdvanceFunc&& advance_while_relevant)
{
    auto& first = get_first(range);
    auto& last = get_last(range);
    advance_while_relevant(first);
    first = changes_tracker.get_new_coord_tolerant(first);

    if (last < BufferCoord{0,0})
        return;

    advance_while_relevant(last);
    last = changes_tracker.get_new_coord_tolerant(last);
}

template<typename RangeContainer>
void update_forward(ConstArrayView<Buffer::Change> changes, RangeContainer& ranges)
{
    ForwardChangesTracker changes_tracker;
    auto advance_while_relevant = [&, it = changes.begin()]
                                  (const BufferCoord& pos) mutable {
        while (it != changes.end() and changes_tracker.relevant(*it, pos))
            changes_tracker.update(*it++);
    };

    auto range_it = std::lower_bound(ranges.begin(), ranges.end(), changes.front(),
                                     [](auto& range, const Buffer::Change& change) { return get_last(range) < change.begin; });
    for (auto end = ranges.end(); range_it != end; ++range_it)
        update_range(changes_tracker, *range_it, advance_while_relevant);
}

template<typename RangeContainer>
void update_backward(ConstArrayView<Buffer::Change> changes, RangeContainer& ranges)
{
    ForwardChangesTracker changes_tracker;
    auto advance_while_relevant = [&, it = changes.rbegin()]
                                  (const BufferCoord& pos) mutable {
        while (it != changes.rend())
        {
            const Buffer::Change change{it->type,
                                        changes_tracker.get_new_coord(it->begin),
                                        changes_tracker.get_new_coord(it->end)};
            if (not changes_tracker.relevant(change, pos))
                break;
            changes_tracker.update(change);
            ++it;
        }
    };

    for (auto& range : ranges)
        update_range(changes_tracker, range, advance_while_relevant);
}

template<typename RangeContainer>
void update_ranges(Buffer& buffer, size_t& timestamp, RangeContainer&& ranges)
{
    if (timestamp == buffer.timestamp())
        return;

    auto changes = buffer.changes_since(timestamp);
    for (auto change_it = changes.begin(); change_it != changes.end(); )
    {
        auto forward_end = forward_sorted_until(change_it, changes.end());
        auto backward_end = backward_sorted_until(change_it, changes.end());

        if (forward_end >= backward_end)
        {
            update_forward({ change_it, forward_end }, ranges);
            change_it = forward_end;
        }
        else
        {
            update_backward({ change_it, backward_end }, ranges);
            change_it = backward_end;
        }
    }
    timestamp = buffer.timestamp();
}

}

#endif // changes_hh_INCLUDED
