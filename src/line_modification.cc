#include "line_modification.hh"

#include "buffer.hh"

namespace Kakoune
{

static LineModification make_line_modif(const Buffer::Change& change)
{
    LineModification res = {};
    res.old_line = change.begin.line;
    res.new_line = change.begin.line;
    if (change.type == Buffer::Change::Insert)
    {
        res.num_added = change.end.line - change.begin.line;
         // inserted a new line at buffer end but end coord is on same line
        if (change.at_end and change.end.column != 0)
            ++res.num_added;
    }
    else
    {
        res.num_removed = change.end.line - change.begin.line;
        // removed last line, but end coord is on same line
        if (change.at_end and change.end.column != 0)
            ++res.num_removed;
    }
    // modified a line
    if (not change.at_end and
        (change.begin.column != 0 or change.end.column != 0))
    {
        ++res.num_removed;
        ++res.num_added;
    }
    return res;
}

Vector<LineModification> compute_line_modifications(const Buffer& buffer, size_t timestamp)
{
    Vector<LineModification> res;
    for (auto& buf_change : buffer.changes_since(timestamp))
    {
        auto change = make_line_modif(buf_change);

        auto pos = std::upper_bound(res.begin(), res.end(), change.new_line,
                                    [](const LineCount& l, const LineModification& c)
                                    { return l < c.new_line; });

        if (pos != res.begin())
        {
            auto& prev = *(pos-1);
            if (change.new_line <= prev.new_line + prev.num_added)
            {
                --pos;
                LineCount removed_from_previously_added_by_pos = clamp(pos->new_line + pos->num_added - change.new_line, 0_line, std::min(pos->num_added, change.num_removed));
                pos->num_removed += change.num_removed - removed_from_previously_added_by_pos;
                pos->num_added += change.num_added - removed_from_previously_added_by_pos;
            }
            else
            {
                change.old_line -= prev.diff();
                pos = res.insert(pos, change);
            }
        }
        else
            pos = res.insert(pos, change);

        auto& modif = *pos;
        auto next = pos + 1;
        if (buf_change.type == Buffer::Change::Erase)
        {
            auto delend = std::upper_bound(next, res.end(), change.new_line + change.num_removed,
                                           [](const LineCount& l, const LineModification& c)
                                           { return l < c.new_line; });

            for (auto it = next; it != delend; ++it)
            {
                LineCount removed_from_previously_added_by_it = std::min(it->num_added, change.new_line + change.num_removed - it->new_line);
                modif.num_removed += it->num_removed - removed_from_previously_added_by_it;
                modif.num_added += it->num_added - removed_from_previously_added_by_it;
            }
            next = res.erase(next, delend);
        }

        auto diff = buf_change.end.line - buf_change.begin.line;
        for (auto it = next; it != res.end(); ++it)
            it->new_line += diff;
    }
    return res;
}

}
