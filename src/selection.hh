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

    // returns min(first, last)
    BufferIterator begin() const;
    // returns max(first, last) + 1
    BufferIterator end() const;

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
// that updates itself when the buffer it points to gets modified.
struct Selection : public Range, public BufferChangeListener
{
    Selection(const BufferIterator& first, const BufferIterator& last,
              CaptureList captures = {});
    Selection(Selection&& other);
    Selection(const Selection& other);
    ~Selection();

    Selection& operator=(const Selection& other);
    Selection& operator=(Selection&& other);

    void avoid_eol();

    CaptureList& captures() { return m_captures; }
    const CaptureList& captures() const { return m_captures; }

private:
    void on_insert(const BufferIterator& begin,
                   const BufferIterator& end) override;
    void on_erase(const BufferIterator& begin,
                  const BufferIterator& end) override;

    void check_invariant() const;

    void register_with_buffer();
    void unregister_with_buffer();

    CaptureList m_captures;
};
using SelectionList = std::vector<Selection>;

}

#endif // selection_hh_INCLUDED

