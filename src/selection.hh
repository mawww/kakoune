#ifndef selection_hh_INCLUDED
#define selection_hh_INCLUDED

#include "buffer.hh"

namespace Kakoune
{

// A Selection holds a buffer range
//
// The Selection class manage a (first, last) buffer iterators pair.
// Selections are oriented, first may be > last, and inclusive.
// Selection updates it's iterators according to modifications made
// in the buffer.
struct Selection : public ModificationListener
{
    Selection(const BufferIterator& first, const BufferIterator& last);
    Selection(const Selection& other);
    ~Selection();

    Selection& operator=(const Selection& other);

    // returns min(first, last)
    BufferIterator begin() const;
    // returns max(first, last) + 1
    BufferIterator end() const;

    const BufferIterator& first() const { return m_first; }
    const BufferIterator& last()  const { return m_last; }

    void merge_with(const Selection& selection);

private:
    BufferIterator m_first;
    BufferIterator m_last;

    void on_modification(const Modification& modification);

    void register_with_buffer();
    void unregister_with_buffer();
};

typedef std::vector<Selection> SelectionList;
typedef std::vector<BufferString> CaptureList;

// Selections are often associated with a capture list
// like when they are created from a regex match with
// capture groups.
struct SelectionAndCaptures
{
    Selection   selection;
    CaptureList captures;

    SelectionAndCaptures(const BufferIterator& first,
                         const BufferIterator& last,
                         CaptureList&& captures_list)
        : selection(first, last), captures(captures_list) {}
    SelectionAndCaptures(const Selection& sel)
        : selection(sel) {}
    SelectionAndCaptures(Selection&& sel)
        : selection(sel) {}
};

typedef std::vector<SelectionAndCaptures> SelectionAndCapturesList;

}

#endif // selection_hh_INCLUDED

