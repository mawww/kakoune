#ifndef selection_hh_INCLUDED
#define selection_hh_INCLUDED

#include "buffer.hh"

namespace Kakoune
{

// An oriented, inclusive buffer range
struct Range
{
public:
    Range(const BufferIterator& first, const BufferIterator& last)
        : m_first{first}, m_last{last} {}

    void merge_with(const Range& range);

    BufferIterator& first() { return m_first; }
    BufferIterator& last() { return m_last; }

    const BufferIterator& first() const { return m_first; }
    const BufferIterator& last() const { return m_last; }

    bool operator== (const Range& other) const
    {
        return m_first == other.m_first and m_last == other.m_last;
    }

    const BufferIterator& min() const { return std::min(m_first, m_last); }
    const BufferIterator& max() const { return std::max(m_first, m_last); }

private:
    BufferIterator m_first;
    BufferIterator m_last;
};

inline bool overlaps(const Range& lhs, const Range& rhs)
{
    return lhs.min() <= rhs.min() ? lhs.max() >= rhs.min()
                                  : lhs.min() <= rhs.max();
}

using CaptureList = std::vector<String>;

// A selection is a Range, associated with a CaptureList
struct Selection : public Range
{
    Selection(const BufferIterator& first, const BufferIterator& last,
              CaptureList captures = {})
        : Range(first, last), m_captures(std::move(captures)) {}

    Selection(const Range& range)
        : Range(range) {}

    CaptureList& captures() { return m_captures; }
    const CaptureList& captures() const { return m_captures; }

private:
    CaptureList m_captures;
};

struct SelectionList : std::vector<Selection>
{
    using std::vector<Selection>::vector;

    void update_insert(const BufferCoord& begin, const BufferCoord& end);
    void update_erase(const BufferCoord& begin, const BufferCoord& end);

    void check_invariant() const;
};

}

#endif // selection_hh_INCLUDED
