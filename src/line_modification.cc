#include "line_modification.hh"

#include "buffer.hh"
#include "unit_tests.hh"

namespace Kakoune
{

static LineModification make_line_modif(const Buffer::Change& change)
{
    LineCount num_added = 0, num_removed = 0;
    if (change.type == Buffer::Change::Insert)
        num_added = change.end.line - change.begin.line;
    else
        num_removed = change.end.line - change.begin.line;
    // modified a line
    if ((change.begin.column != 0 or change.end.column != 0))
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

            if (diff != 0)
            {
                for (auto it = next; it != res.end(); ++it)
                    it->new_line -= diff;
            }
        }
        else if (diff != 0)
        {
            for (auto it = next; it != res.end(); ++it)
                it->new_line += diff;
        }
    }
    return res;
}

bool operator==(const LineModification& lhs, const LineModification& rhs)
{
    return lhs.old_line == rhs.old_line and lhs.new_line == rhs.new_line and
           lhs.num_removed == rhs.num_removed and lhs.num_added == rhs.num_added;
}

void LineRangeSet::update(ConstArrayView<LineModification> modifs)
{
    if (modifs.empty())
        return;

    for (auto it = begin(); it != end(); ++it)
    {
        auto modif_beg = std::lower_bound(modifs.begin(), modifs.end(), it->begin,
                                          [](const LineModification& c, const LineCount& l)
                                          { return c.old_line + c.num_removed < l; });
        auto modif_end = std::upper_bound(modifs.begin(), modifs.end(), it->end,
                                          [](const LineCount& l, const LineModification& c)
                                          { return l < c.old_line; });

        if (modif_beg == modifs.end())
        {
            const auto diff = (modif_beg-1)->diff();
            it->begin += diff;
            it->end += diff;
            continue;
        }

        const auto diff = modif_beg->new_line - modif_beg->old_line;
        it->begin += diff;
        it->end += diff;

        while (modif_beg != modif_end)
        {
            auto& m = *modif_beg++;
            if (m.num_removed > 0)
            {
                if (m.new_line < it->begin)
                    it->begin = std::max(m.new_line, it->begin - m.num_removed);
                it->end = std::max(m.new_line, std::max(it->begin, it->end - m.num_removed));
            }
            if (m.num_added > 0)
            {
                if (it->begin >= m.new_line)
                    it->begin += m.num_added;
                else
                {
                    it = insert(it, {it->begin, m.new_line}) + 1;
                    it->begin = m.new_line + m.num_added;
                }
                it->end += m.num_added;
            }
        }
    };
    erase(remove_if(*this, [](auto& r) { return r.begin >= r.end; }), end());
}

void LineRangeSet::add_range(LineRange range, FunctionRef<void (LineRange)> on_new_range)
{
    auto insert_at = std::lower_bound(begin(), end(), range.begin,
                                      [](LineRange range, LineCount line) { return range.end < line; });
    if (insert_at == end() or insert_at->begin > range.end)
        on_new_range(range);
    else
    {
        auto pos = range.begin;
        auto it = insert_at;
        for (; it != end() and it->begin <= range.end; ++it)
        {
            if (pos < it->begin)
                on_new_range({pos, it->begin});

            range = LineRange{std::min(range.begin, it->begin), std::max(range.end, it->end)};
            pos = it->end;
        }
        insert_at = erase(insert_at, it);
        if (pos < range.end)
            on_new_range({pos, range.end});
    }
    insert(insert_at, range);
}

void LineRangeSet::remove_range(LineRange range)
{
    auto inside = [](LineCount line, LineRange range) {
        return range.begin <= line and line < range.end;
    };

    auto it = std::lower_bound(begin(), end(), range.begin,
                               [](LineRange range, LineCount line) { return range.end < line; });
    if (it == end() or it->begin > range.end)
        return;
    else while (it != end() and it->begin <= range.end)
    {
        if (it->begin < range.begin and range.end <= it->end)
        {
            it = insert(it, {it->begin, range.begin}) + 1;
            it->begin = range.end;
        }
        if (inside(it->begin, range))
            it->begin = range.end;
        if (inside(it->end, range))
            it->end = range.begin;

        if (it->end <= it->begin)
            it = erase(it);
        else
            ++it;
    }
}


UnitTest test_line_modifications{[]()
{
    auto make_lines = [](auto&&... lines) { return BufferLines{StringData::create({lines})...}; };

    {
        Buffer buffer("test", Buffer::Flags::None, make_lines("line 1\n", "line 2\n"));
        auto ts = buffer.timestamp();
        buffer.erase({1, 0}, {2, 0});

        auto modifs = compute_line_modifications(buffer, ts);
        kak_assert(modifs.size() == 1 and modifs[0] == LineModification{ 1, 1, 1, 0 });
    }

    {
        Buffer buffer("test", Buffer::Flags::None, make_lines("line 1\n", "line 2\n"));
        auto ts = buffer.timestamp();
        buffer.insert({2, 0}, "line 3");

        auto modifs = compute_line_modifications(buffer, ts);
        kak_assert(modifs.size() == 1 and modifs[0] == LineModification{ 2, 2, 0, 1 });
    }

    {
        Buffer buffer("test", Buffer::Flags::None, make_lines("line 1\n", "line 2\n", "line 3\n"));

        auto ts = buffer.timestamp();
        buffer.insert({1, 4}, "hoho\nhehe");
        buffer.erase({0, 0}, {1, 0});

        auto modifs = compute_line_modifications(buffer, ts);
        kak_assert(modifs.size() == 1 and modifs[0] == LineModification{ 0, 0, 2, 2 });
    }

    {
        Buffer buffer("test", Buffer::Flags::None, make_lines("line 1\n", "line 2\n", "line 3\n", "line 4\n"));

        auto ts = buffer.timestamp();
        buffer.erase({0,0}, {3,0});
        buffer.insert({1,0}, "newline 1\nnewline 2\nnewline 3\n");
        buffer.erase({0,0}, {1,0});
        {
            auto modifs = compute_line_modifications(buffer, ts);
            kak_assert(modifs.size() == 1 and modifs[0] == LineModification{ 0, 0, 4, 3 });
        }
        buffer.insert({3,0}, "newline 4\n");

        {
            auto modifs = compute_line_modifications(buffer, ts);
            kak_assert(modifs.size() == 1 and modifs[0] == LineModification{ 0, 0, 4, 4 });
        }
    }

    {
        Buffer buffer("test", Buffer::Flags::None, make_lines("line 1\n"));
        auto ts = buffer.timestamp();
        buffer.insert({0,0}, "n");
        buffer.insert({0,1}, "e");
        buffer.insert({0,2}, "w");
        auto modifs = compute_line_modifications(buffer, ts);
        kak_assert(modifs.size() == 1 and modifs[0] == LineModification{ 0, 0, 1, 1 });
    }
}};

UnitTest test_line_range_set{[]{
    auto expect = [](ConstArrayView<LineRange> ranges) {
        return [it = ranges.begin(), end = ranges.end()](LineRange r) mutable {
            kak_assert(it != end);
            kak_assert(r == *it++);
        };
    };

    {
        LineRangeSet ranges;
        ranges.add_range({0, 5}, expect({{0, 5}}));
        ranges.add_range({10, 15}, expect({{10, 15}}));
        ranges.add_range({5, 10}, expect({{5, 10}}));
        kak_assert((ranges.view() == ConstArrayView<LineRange>{{0, 15}}));
        ranges.add_range({5, 10}, expect({}));
        ranges.remove_range({3, 8});
        kak_assert((ranges.view() == ConstArrayView<LineRange>{{0, 3}, {8, 15}}));
    }
    {
        LineRangeSet ranges;
        ranges.add_range({0, 7}, expect({{0, 7}}));
        ranges.add_range({9, 15}, expect({{9, 15}}));
        ranges.add_range({5, 10}, expect({{7, 9}}));
        kak_assert((ranges.view() == ConstArrayView<LineRange>{{0, 15}}));
    }
    {
        LineRangeSet ranges;
        ranges.add_range({0, 7}, expect({{0, 7}}));
        ranges.add_range({11, 15}, expect({{11, 15}}));
        ranges.add_range({5, 10}, expect({{7, 10}}));
        kak_assert((ranges.view() == ConstArrayView<LineRange>{{0, 10}, {11, 15}}));
        ranges.remove_range({8, 13});
        kak_assert((ranges.view() == ConstArrayView<LineRange>{{0, 8}, {13, 15}}));
    }
    {
        LineRangeSet ranges;
        ranges.add_range({0, 5}, expect({{0, 5}}));
        ranges.add_range({10, 15}, expect({{10, 15}}));
        ranges.update(ConstArrayView<LineModification>{{3, 3, 3, 1}, {11, 9, 2, 4}});
        kak_assert((ranges.view() == ConstArrayView<LineRange>{{0, 3}, {8, 9}, {13, 15}}));
    }
    {
        LineRangeSet ranges;
        ranges.add_range({0, 5}, expect({{0, 5}}));
        ranges.update(ConstArrayView<LineModification>{{2, 2, 2, 0}});
        kak_assert((ranges.view() == ConstArrayView<LineRange>{{0, 3}}));
    }
    {
        LineRangeSet ranges;
        ranges.add_range({0, 5}, expect({{0, 5}}));
        ranges.update(ConstArrayView<LineModification>{{2, 2, 0, 2}});
        kak_assert((ranges.view() == ConstArrayView<LineRange>{{0, 2}, {4, 7}}));
    }
    {
        LineRangeSet ranges;
        ranges.add_range({0, 1}, expect({{0, 1}}));
        ranges.add_range({5, 10}, expect({{5, 10}}));
        ranges.add_range({15, 20}, expect({{15, 20}}));
        ranges.add_range({25, 30}, expect({{25, 30}}));
        ranges.update(ConstArrayView<LineModification>{{2, 2, 3, 0}});
        kak_assert((ranges.view() == ConstArrayView<LineRange>{{0, 1}, {2, 7}, {12, 17}, {22, 27}}));
    }
}};

}
