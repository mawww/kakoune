#include "line_modification.hh"

#include "buffer.hh"
#include "unit_tests.hh"

namespace Kakoune
{

static LineModification make_line_modif(const Buffer::Change& change)
{
    LineCount num_added = 0, num_removed = 0;
    if (change.type == Buffer::Change::Insert)
    {
        num_added = change.end.line - change.begin.line;
         // inserted a new line at buffer end but end coord is on same line
        if (change.at_end and change.end.column != 0)
            ++num_added;
    }
    else
    {
        num_removed = change.end.line - change.begin.line;
        // removed last line, but end coord is on same line
        if (change.at_end and change.end.column != 0)
            ++num_removed;
    }
    // modified a line
    if (not change.at_end and
        (change.begin.column != 0 or change.end.column != 0))
    {
        ++num_removed;
        ++num_added;
    }
    return { change.begin.line, change.begin.line, num_removed, num_added };
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
                const LineCount removed_from_previously_added_by_pos =
                    clamp(pos->new_line + pos->num_added - change.new_line,
                          0_line, std::min(pos->num_added, change.num_removed));

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

        auto next = pos + 1;
        auto diff = buf_change.end.line - buf_change.begin.line;
        if (buf_change.type == Buffer::Change::Erase)
        {
            auto delend = std::upper_bound(next, res.end(), change.new_line + change.num_removed,
                                           [](const LineCount& l, const LineModification& c)
                                           { return l < c.new_line; });

            for (auto it = next; it != delend; ++it)
            {
                const LineCount removed_from_previously_added_by_it =
                    std::min(it->num_added, change.new_line + change.num_removed - it->new_line);

                pos->num_removed += it->num_removed - removed_from_previously_added_by_it;
                pos->num_added += it->num_added - removed_from_previously_added_by_it;
            }
            next = res.erase(next, delend);

            for (auto it = next; it != res.end(); ++it)
                it->new_line -= diff;
        }
        else
        {
            for (auto it = next; it != res.end(); ++it)
                it->new_line += diff;
        }
    }
    return res;
}

bool operator==(const LineModification& lhs, const LineModification& rhs)
{
    return std::tie(lhs.old_line, lhs.new_line, lhs.num_removed, lhs.num_added) ==
           std::tie(rhs.old_line, rhs.new_line, rhs.num_removed, rhs.num_added);
}

UnitTest test_line_modifications{[]()
{
    {
        Buffer buffer("test", Buffer::Flags::None, { "line 1\n"_ss, "line 2\n"_ss });
        auto ts = buffer.timestamp();
        buffer.erase(buffer.iterator_at({1, 0}), buffer.iterator_at({2, 0}));

        auto modifs = compute_line_modifications(buffer, ts);
        kak_assert(modifs.size() == 1 && modifs[0] == LineModification{ 1 COMMA 1 COMMA 1 COMMA 0 });
    }

    {
        Buffer buffer("test", Buffer::Flags::None, { "line 1\n"_ss, "line 2\n"_ss });
        auto ts = buffer.timestamp();
        buffer.insert(buffer.iterator_at({1, 7}), "line 3");

        auto modifs = compute_line_modifications(buffer, ts);
        kak_assert(modifs.size() == 1 && modifs[0] == LineModification{ 2 COMMA 2 COMMA 0 COMMA 1 });
    }

    {
        Buffer buffer("test", Buffer::Flags::None,
                      { "line 1\n"_ss, "line 2\n"_ss, "line 3\n"_ss });

        auto ts = buffer.timestamp();
        buffer.insert(buffer.iterator_at({1, 4}), "hoho\nhehe");
        buffer.erase(buffer.iterator_at({0, 0}), buffer.iterator_at({1, 0}));

        auto modifs = compute_line_modifications(buffer, ts);
        kak_assert(modifs.size() == 1 && modifs[0] == LineModification{ 0 COMMA 0 COMMA 2 COMMA 2 });
    }

    {
        Buffer buffer("test", Buffer::Flags::None,
                      { "line 1\n"_ss, "line 2\n"_ss, "line 3\n"_ss, "line 4\n"_ss });

        auto ts = buffer.timestamp();
        buffer.erase(buffer.iterator_at({0,0}), buffer.iterator_at({3,0}));
        buffer.insert(buffer.iterator_at({1,0}), "newline 1\nnewline 2\nnewline 3\n");
        buffer.erase(buffer.iterator_at({0,0}), buffer.iterator_at({1,0}));
        {
            auto modifs = compute_line_modifications(buffer, ts);
            kak_assert(modifs.size() == 1 && modifs[0] == LineModification{ 0 COMMA 0 COMMA 4 COMMA 3 });
        }
        buffer.insert(buffer.iterator_at({3,0}), "newline 4\n");

        {
            auto modifs = compute_line_modifications(buffer, ts);
            kak_assert(modifs.size() == 1 && modifs[0] == LineModification{ 0 COMMA 0 COMMA 4 COMMA 4 });
        }
    }

    {
        Buffer buffer("test", Buffer::Flags::None, { "line 1\n"_ss });
        auto ts = buffer.timestamp();
        buffer.insert(buffer.iterator_at({0,0}), "n");
        buffer.insert(buffer.iterator_at({0,1}), "e");
        buffer.insert(buffer.iterator_at({0,2}), "w");
        auto modifs = compute_line_modifications(buffer, ts);
        kak_assert(modifs.size() == 1 && modifs[0] == LineModification{ 0 COMMA 0 COMMA 1 COMMA 1 });
    }
}};

}
