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

template<typename RangeContainer>
void update_forward(ConstArrayView<Buffer::Change> changes, RangeContainer& ranges)
{
    ForwardChangesTracker changes_tracker;

    auto change_it = changes.begin();
    auto advance_while_relevant = [&](const BufferCoord& pos) mutable {
        while (change_it != changes.end() and changes_tracker.relevant(*change_it, pos))
            changes_tracker.update(*change_it++);
    };

    for (auto& range : ranges)
    {
        auto& first = get_first(range);
        auto& last = get_last(range);
        advance_while_relevant(first);
        first = changes_tracker.get_new_coord_tolerant(first);

        advance_while_relevant(last);
        last = changes_tracker.get_new_coord_tolerant(last);
    }
}

template<typename RangeContainer>
void update_backward(ConstArrayView<Buffer::Change> changes, RangeContainer& ranges)
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

    for (auto& range : ranges)
    {
        auto& first = get_first(range);
        auto& last = get_last(range);
        advance_while_relevant(first);
        first = changes_tracker.get_new_coord_tolerant(first);

        advance_while_relevant(last);
        last = changes_tracker.get_new_coord_tolerant(last);
    }
}

}

#endif // changes_hh_INCLUDED
