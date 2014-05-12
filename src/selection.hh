#ifndef selection_hh_INCLUDED
#define selection_hh_INCLUDED

#include "buffer.hh"

namespace Kakoune
{

using CaptureList = std::vector<String>;

// A selection is a Selection, associated with a CaptureList
struct Selection
{
    Selection() = default;
    explicit Selection(ByteCoord pos) : Selection(pos,pos) {}
    Selection(ByteCoord anchor, ByteCoord cursor,
              CaptureList captures = {})
        : m_anchor{anchor}, m_cursor{cursor},
          m_captures(std::move(captures)) {}

    void merge_with(const Selection& range);

    ByteCoord& anchor() { return m_anchor; }
    ByteCoord& cursor() { return m_cursor; }

    const ByteCoord& anchor() const { return m_anchor; }
    const ByteCoord& cursor() const { return m_cursor; }

    CaptureList& captures() { return m_captures; }
    const CaptureList& captures() const { return m_captures; }

    bool operator== (const Selection& other) const
    {
        return m_anchor == other.m_anchor and m_cursor == other.m_cursor;
    }

    const ByteCoord& min() const { return std::min(m_anchor, m_cursor); }
    const ByteCoord& max() const { return std::max(m_anchor, m_cursor); }

private:
    ByteCoord m_anchor;
    ByteCoord m_cursor;

    CaptureList m_captures;
};

inline bool overlaps(const Selection& lhs, const Selection& rhs)
{
    return lhs.min() <= rhs.min() ? lhs.max() >= rhs.min()
                                  : lhs.min() <= rhs.max();
}

static bool compare_selections(const Selection& lhs, const Selection& rhs)
{
    return lhs.min() < rhs.min();
}

struct SelectionList
{
    SelectionList() = default;
    SelectionList(ByteCoord c) : m_selections{Selection{c,c}} {}
    SelectionList(Selection s) : m_selections{s} {}

    void update_insert(ByteCoord begin, ByteCoord end, bool at_end);
    void update_erase(ByteCoord begin, ByteCoord end, bool at_end);

    void check_invariant() const;

    const Selection& main() const { return (*this)[m_main]; }
    Selection& main() { return (*this)[m_main]; }
    size_t main_index() const { return m_main; }
    void set_main_index(size_t main) { kak_assert(main < size()); m_main = main; }

    void rotate_main(int count) { m_main = (m_main + count) % size(); }

    void push_back(const Selection& sel) { m_selections.push_back(sel); }
    void push_back(Selection&& sel) { m_selections.push_back(std::move(sel)); }

    Selection& operator[](size_t i) { return m_selections[i]; }
    const Selection& operator[](size_t i) const { return m_selections[i]; }

    using iterator = std::vector<Selection>::iterator;
    iterator begin() { return m_selections.begin(); }
    iterator end() { return m_selections.end(); }

    using const_iterator = std::vector<Selection>::const_iterator;
    const_iterator begin() const { return m_selections.begin(); }
    const_iterator end() const { return m_selections.end(); }

    template<typename... Args>
    iterator insert(Args... args)
    {
        return m_selections.insert(std::forward<Args>(args)...);
    }

    template<typename... Args>
    iterator erase(Args... args)
    {
        return m_selections.erase(std::forward<Args>(args)...);
    }

    size_t size() const { return m_selections.size(); }
    bool empty() const { return m_selections.empty(); }

    bool operator==(const SelectionList& other) const { return m_selections == other.m_selections; }
    bool operator!=(const SelectionList& other) const { return m_selections != other.m_selections; }

    template<typename OverlapsFunc>
    void merge_overlapping(OverlapsFunc overlaps)
    {
        kak_assert(std::is_sorted(begin(), end(), compare_selections));
        size_t i = 0;
        for (size_t j = 1; j < size(); ++j)
        {
            if (overlaps((*this)[i], (*this)[j]))
            {
                (*this)[i].merge_with((*this)[j]);
                if (i < m_main)
                    --m_main;
            }
            else
            {
                ++i;
                if (i != j)
                    (*this)[i] = std::move((*this)[j]);
            }
        }
        erase(begin() + i + 1, end());
        kak_assert(std::is_sorted(begin(), end(), compare_selections));
    }

    void sort_and_merge_overlapping()
    {
        if (size() == 1)
            return;

        const auto& main = this->main();
        const auto main_begin = main.min();
        m_main = std::count_if(begin(), end(), [&](const Selection& sel) {
                                   auto begin = sel.min();
                                   if (begin == main_begin)
                                       return &sel < &main;
                                   else
                                       return begin < main_begin;
                               });
        std::stable_sort(begin(), end(), compare_selections);
        merge_overlapping(overlaps);
    }

private:
    size_t m_main = 0;
    std::vector<Selection> m_selections;
};

}

#endif // selection_hh_INCLUDED
