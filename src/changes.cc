#include "changes.hh"

namespace Kakoune
{
void ForwardChangesTracker::update(const Buffer::Change& change)
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

void ForwardChangesTracker::update(const Buffer& buffer, size_t& timestamp)
{
    for (auto& change : buffer.changes_since(timestamp))
        update(change);
    timestamp = buffer.timestamp();
}

BufferCoord ForwardChangesTracker::get_old_coord(BufferCoord coord) const
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

BufferCoord ForwardChangesTracker::get_new_coord(BufferCoord coord) const
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

BufferCoord ForwardChangesTracker::get_new_coord_tolerant(BufferCoord coord) const
{
    if (coord < old_pos)
        return cur_pos;
    return get_new_coord(coord);
}

bool ForwardChangesTracker::relevant(const Buffer::Change& change, BufferCoord old_coord) const
{
    auto new_coord = get_new_coord_tolerant(old_coord);
    return change.type == Buffer::Change::Insert ? change.begin <= new_coord
                                                 : change.begin < new_coord;
}

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
            if (first->begin < next->end)
                return next;
            first = next;
        }
    }
    return last;
}

}
