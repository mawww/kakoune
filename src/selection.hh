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

    // returns min(first, last)
    BufferIterator begin() const;
    // returns max(first, last) + 1
    BufferIterator end() const;

    String content() const;

    void check_invariant() const;
private:
    BufferIterator m_first;
    BufferIterator m_last;
};

inline bool overlaps(const Range& lhs, const Range& rhs)
{
    return (lhs.first() <= rhs.first() and lhs.last() >= rhs.first()) or
           (lhs.first() <= rhs.last()  and lhs.last() >= rhs.last());
}

using CaptureList = std::vector<String>;

// A selection is a Range, associated with a CaptureList
struct Selection : public Range
{
    Selection(const BufferIterator& first, const BufferIterator& last,
              CaptureList captures = {})
        : Range(first, last), m_captures(std::move(captures)) {}

    void avoid_eol();

    CaptureList& captures() { return m_captures; }
    const CaptureList& captures() const { return m_captures; }

    const Buffer& buffer() const { return first().buffer(); }
private:

    CaptureList m_captures;
};
using SelectionList = std::vector<Selection>;

}

#endif // selection_hh_INCLUDED
