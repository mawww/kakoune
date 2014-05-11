#include "line_change_watcher.hh"

#include "buffer.hh"

namespace Kakoune
{

namespace
{

struct Change
{
    LineCount pos;
    LineCount num;
};

std::vector<Change> compute_changes(const Buffer& buffer, size_t timestamp)
{
    std::vector<Change> res;
    for (auto& change : buffer.changes_since(timestamp))
    {
        ByteCoord begin = change.begin;
        ByteCoord end = change.end;
        if (change.type == Buffer::Change::Insert)
        {
            if (change.at_end and begin != ByteCoord{0,0})
            {
                kak_assert(begin.column == 0);
                --begin.line;
            }
            res.push_back({begin.line, end.line - begin.line});
        }
        else
        {
            if (change.at_end and begin != ByteCoord{0,0})
            {
                kak_assert(begin.column == 0);
                --begin.line;
            }
            res.push_back({begin.line, begin.line - end.line});
        }
    }
    return res;
}

}

LineChangeWatcher::LineChangeWatcher(const Buffer& buffer)
    : m_buffer(&buffer), m_timestamp(buffer.timestamp()) {}

std::vector<LineModification> LineChangeWatcher::compute_modifications()
{
    std::vector<LineModification> res;
    for (auto& change : compute_changes(*m_buffer, m_timestamp))
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

            const LineCount num_added_after_pos =
                modif.new_line + modif.num_added - change.pos;
            const LineCount num_removed_from_added =
                std::min(num_removed, num_added_after_pos);
            modif.num_added -= num_removed_from_added;
            modif.num_removed += num_removed - num_removed_from_added;

            for (auto it = next; it != res.end(); ++it)
                it->new_line -= num_removed;
        }
    }
    m_timestamp = m_buffer->timestamp();
    return res;
}

}
