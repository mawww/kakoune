#include "modification.hh"

#include "buffer.hh"

namespace Kakoune
{

namespace
{

ByteCount change_added_column(const Buffer::Change& change)
{
    kak_assert(change.type == Buffer::Change::Insert);
    if (change.begin.line == change.end.line)
        return change.end.column - change.begin.column;
    else
        return change.end.column;
}

}

ByteCoord Modification::added_end() const
{
    if (num_added.line)
        return { new_coord.line + num_added.line, num_added.column };
    else
        return { new_coord.line, new_coord.column + num_added.column };
}

ByteCoord Modification::removed_end() const
{
    if (num_removed.line)
        return { old_coord.line + num_removed.line, num_removed.column };
    else
        return { old_coord.line, old_coord.column + num_removed.column };
}

ByteCoord Modification::get_old_coord(ByteCoord coord) const
{
    Modification inverse = { new_coord, old_coord, num_added, num_removed };
    return inverse.get_new_coord(coord);
}

ByteCoord Modification::get_new_coord(ByteCoord coord) const
{
    if (coord < old_coord)
        return coord;

    // apply remove
    if (coord < removed_end())
        coord = old_coord;
    else if (coord.line == old_coord.line + num_removed.line)
    {
        coord.line = old_coord.line;
        if (num_removed.line != 0)
            coord.column += old_coord.column;
        coord.column -= num_removed.column;
    }
    else
        coord.line -= num_removed.line;

    // apply move
    coord.line += new_coord.line - old_coord.line;
    if (coord.line == new_coord.line)
        coord.column += new_coord.column - old_coord.column;

    // apply add
    if (coord.line == new_coord.line)
    {
        if (num_added.line == 0)
            coord.column += num_added.column;
        else
            coord.column += num_added.column - new_coord.column;
    }
    coord.line += num_added.line;

    return coord;
}

std::vector<Modification> compute_modifications(memoryview<Buffer::Change> changes)
{
    std::vector<Modification> res;
    for (auto& change : changes)
    {
        auto pos = std::upper_bound(res.begin(), res.end(), change.begin,
                                    [](const ByteCoord& l, const Modification& c)
                                    { return l < c.new_coord; });

        if (pos != res.begin())
        {
            auto& prev = *(pos-1);
            if (change.begin <= prev.added_end())
                --pos;
            else
                pos = res.insert(pos, {prev.get_old_coord(change.begin), change.begin, {}, {}});
        }
        else
            pos = res.insert(pos, {change.begin, change.begin, {}, {}});

        auto& modif = *pos;
        auto next = pos + 1;
        if (change.type == Buffer::Change::Insert)
        {
            const LineCount last_line = modif.new_coord.line + modif.num_added.line;

            modif.num_added.line += change.end.line - change.begin.line;

            if (change.begin.line == last_line)
            {
                if (change.end.line == change.begin.line)
                    modif.num_added.column += change.end.column - change.begin.column;
                else
                    modif.num_added.column = change.end.column;
                kak_assert(modif.num_added.column >= 0);
            }

            for (auto it = next; it != res.end(); ++it)
            {
                if (it->new_coord.line == change.begin.line)
                    it->new_coord.column += change.end.column - change.begin.column;
                it->new_coord.line += change.end.line - change.begin.line;

#ifdef KAK_DEBUG
                auto ref_new_coord = (it-1)->get_new_coord(it->old_coord);
                kak_assert(it->new_coord == ref_new_coord);
#endif
            }
        }
        else
        {
            ByteCoord num_removed = { change.end.line - change.begin.line, 0 };
            if (num_removed.line != 0)
                num_removed.column = change.end.column;
            else
                num_removed.column = change.end.column - change.begin.column;

            auto delend = std::upper_bound(next, res.end(), change.end,
                                           [](const ByteCoord& l, const Modification& c)
                                           { return l < c.new_coord; });

            for (auto it = next; it != delend; ++it)
            {
                {
                    LineCount removed_from_it = change.end.line - it->new_coord.line;
                    modif.num_removed.line += it->num_removed.line - std::min(removed_from_it, it->num_added.line);
                    modif.num_added.line += std::max(0_line, it->num_added.line - removed_from_it);
                }

                if (it->new_coord.line == change.end.line)
                {
                    ByteCount removed_from_it = num_removed.column - it->new_coord.column;
                    modif.num_removed.column += it->num_removed.column - std::min(removed_from_it, it->num_added.column);
                    modif.num_added.column += std::max(0_byte, it->num_added.column - removed_from_it);
                }
            }
            next = res.erase(next, delend);

            ByteCoord num_added_after_pos = { modif.new_coord.line + modif.num_added.line - change.begin.line, 0 };
            if (change.begin.line == modif.new_coord.line + modif.num_added.line)
            {
                if (modif.num_added.line == 0)
                    num_added_after_pos.column = modif.new_coord.column + modif.num_added.column - change.begin.column;
                else
                    num_added_after_pos.column = modif.num_added.column - change.begin.column;
            }
            ByteCoord num_removed_from_added = std::min(num_removed, num_added_after_pos);
            modif.num_added -= num_removed_from_added;
            modif.num_removed += num_removed - num_removed_from_added;

            for (auto it = next; it != res.end(); ++it)
            {
                if (it->new_coord.line == change.end.line)
                    it->new_coord.column += change.begin.column - change.end.column;
                it->new_coord.line += change.begin.line - change.end.line;

#ifdef KAK_DEBUG
                auto ref_new_coord = (it-1)->get_new_coord(it->old_coord);
                kak_assert(it->new_coord == ref_new_coord);
#endif
            }
        }
    }

#ifdef KAK_DEBUG
    for (size_t i = 0; i+1 < res.size(); ++i)
    {
        auto old_coord = res[i].get_old_coord(res[i+1].new_coord);
        kak_assert(res[i+1].old_coord == old_coord);
        auto new_coord = res[i].get_new_coord(res[i+1].old_coord);
        kak_assert(res[i+1].new_coord == new_coord);
    }
#endif

    return res;
}

std::vector<Modification> compute_modifications(const Buffer& buffer, size_t timestamp)
{
    return compute_modifications(buffer.changes_since(timestamp));
}

ByteCoord update_pos(memoryview<Modification> modifs, ByteCoord pos)
{
    auto modif_it = std::upper_bound(modifs.begin(), modifs.end(), pos,
                                     [](const ByteCoord& c, const Modification& m)
                                     { return c < m.old_coord; });
    if (modif_it != modifs.begin())
    {
        auto& prev = *(modif_it-1);
        return prev.get_new_coord(pos);
    }
    return pos;
}

}
