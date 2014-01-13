#ifndef selection_hh_INCLUDED
#define selection_hh_INCLUDED

#include "buffer.hh"

namespace Kakoune
{

// An oriented, inclusive buffer range
struct Range
{
public:
    Range(BufferCoord first, BufferCoord last)
        : m_first{first}, m_last{last} {}

    void merge_with(const Range& range);

    BufferCoord& first() { return m_first; }
    BufferCoord& last() { return m_last; }

    const BufferCoord& first() const { return m_first; }
    const BufferCoord& last() const { return m_last; }

    bool operator== (const Range& other) const
    {
        return m_first == other.m_first and m_last == other.m_last;
    }

    const BufferCoord& min() const { return std::min(m_first, m_last); }
    const BufferCoord& max() const { return std::max(m_first, m_last); }

private:
    BufferCoord m_first;
    BufferCoord m_last;
};

inline bool overlaps(const Range& lhs, const Range& rhs)
{
    return lhs.min() <= rhs.min() ? lhs.max() >= rhs.min()
                                  : lhs.min() <= rhs.max();
}

inline String content(const Buffer& buffer, const Range& range)
{
    return buffer.string(range.min(), buffer.char_next(range.max()));
}

inline BufferIterator erase(Buffer& buffer, const Range& range)
{
    return buffer.erase(buffer.iterator_at(range.min()),
                        utf8::next(buffer.iterator_at(range.max())));
}

inline CharCount char_length(const Buffer& buffer, const Range& range)
{
    return utf8::distance(buffer.iterator_at(range.min()),
                          utf8::next(buffer.iterator_at(range.max())));
}


inline void avoid_eol(const Buffer& buffer, BufferCoord& coord)
{
    const auto column = coord.column;
    const auto& line = buffer[coord.line];
    if (column != 0 and column == line.length() - 1)
        coord.column = line.byte_count_to(line.char_length() - 2);
}

inline void avoid_eol(const Buffer& buffer, Range& sel)
{
    avoid_eol(buffer, sel.first());
    avoid_eol(buffer, sel.last());
}


using CaptureList = std::vector<String>;

// A selection is a Range, associated with a CaptureList
struct Selection : public Range
{
    explicit Selection(BufferCoord pos) : Range(pos,pos) {}
    Selection(BufferCoord first, BufferCoord last,
              CaptureList captures = {})
        : Range(first, last), m_captures(std::move(captures)) {}

    Selection(const Range& range)
        : Range(range) {}

    CaptureList& captures() { return m_captures; }
    const CaptureList& captures() const { return m_captures; }

private:
    CaptureList m_captures;
};

static bool compare_selections(const Selection& lhs, const Selection& rhs)
{
    return lhs.min() < rhs.min();
}

struct SelectionList : std::vector<Selection>
{
    SelectionList() = default;
    SelectionList(BufferCoord c) : std::vector<Selection>{Selection{c,c}} {}
    SelectionList(Selection s) : std::vector<Selection>{s} {}

    void update_insert(const Buffer& buffer, BufferCoord begin, BufferCoord end);
    void update_erase(const Buffer& buffer, BufferCoord begin, BufferCoord end);

    void check_invariant() const;

    const Selection& main() const { return (*this)[m_main]; }
    Selection& main() { return (*this)[m_main]; }
    size_t main_index() const { return m_main; }
    void set_main_index(size_t main) { kak_assert(main < size()); m_main = main; }

    void rotate_main(int count) { m_main = (m_main + count) % size(); }

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
};

}

#endif // selection_hh_INCLUDED
