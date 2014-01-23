#include "line_change_watcher.hh"

namespace Kakoune
{

std::vector<LineModification> LineChangeWatcher::compute_modifications()
{
    std::vector<LineModification> res;
    for (auto& change : m_changes)
    {
        auto pos = std::upper_bound(res.begin(), res.end(), change.pos,
                                    [](const LineCount& l, const LineModification& c)
                                    { return l < c.new_line; });

        if (pos != res.begin())
        {
            auto& prev = *(pos-1);
            if (change.pos <= prev.new_line + prev.num_added)
                --pos;
            else
                pos = res.insert(pos, {change.pos - prev.diff(), change.pos, 0, 0});
        }
        else
            pos = res.insert(pos, {change.pos, change.pos, 0, 0});

        auto& modif = *pos;
        auto next = pos + 1;
        if (change.num > 0)
        {
            modif.num_added += change.num;
            for (auto it = next; it != res.end(); ++it)
                it->new_line += change.num;
        }
        if (change.num < 0)
        {
            const LineCount num_removed = -change.num;

            auto delend = std::upper_bound(next, res.end(), change.pos + num_removed,
                                           [](const LineCount& l, const LineModification& c)
                                           { return l < c.new_line; });

            for (auto it = next; it != delend; ++it)
            {
                LineCount removed_from_it = (change.pos + num_removed - it->new_line);
                modif.num_removed += it->num_removed - std::min(removed_from_it, it->num_added);
                modif.num_added += std::max(0_line, it->num_added - removed_from_it);
            }
            next = res.erase(next, delend);

            const LineCount num_removed_from_added = std::min(num_removed, modif.new_line + modif.num_added - change.pos);
            modif.num_added -= num_removed_from_added;
            modif.num_removed += num_removed - num_removed_from_added;

            for (auto it = next; it != res.end(); ++it)
                it->new_line -= num_removed;
        }
    }
    m_changes.clear();
    return res;
}

void LineChangeWatcher::on_insert(const Buffer& buffer, BufferCoord begin, BufferCoord end)
{
    m_changes.push_back({begin.line, end.line - begin.line});
}

void LineChangeWatcher::on_erase(const Buffer& buffer, BufferCoord begin, BufferCoord end)
{
    if (begin.line == buffer.line_count())
    {
        kak_assert(begin.column == 0);
        --begin.line;
    }
    m_changes.push_back({begin.line, begin.line - end.line});
}

}
