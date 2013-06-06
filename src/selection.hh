#ifndef selection_hh_INCLUDED
#define selection_hh_INCLUDED

#include "buffer.hh"

namespace Kakoune
{

// An oriented, inclusive buffer range
struct Range
{
public:
    Range(const BufferCoord& first, const BufferCoord& last)
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

using CaptureList = std::vector<String>;

// A selection is a Range, associated with a CaptureList
struct Selection : public Range
{
    Selection(const BufferCoord& first, const BufferCoord& last,
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

    void update_insert(const Buffer& buffer, const BufferCoord& begin, const BufferCoord& end);
    void update_erase(const Buffer& buffer, const BufferCoord& begin, const BufferCoord& end);

    void check_invariant() const;
};

}

#endif // selection_hh_INCLUDED
