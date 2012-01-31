#ifndef selection_hh_INCLUDED
#define selection_hh_INCLUDED

#include "buffer.hh"

namespace Kakoune
{

struct Selection : public ModificationListener
{
    typedef std::vector<BufferString> CaptureList;

    Selection(const BufferIterator& first, const BufferIterator& last,
              const CaptureList& captures = CaptureList());

    Selection(const BufferIterator& first, const BufferIterator& last,
              CaptureList&& captures);

    Selection(const Selection& other);
    Selection(Selection&& other);

    ~Selection();

    Selection& operator=(const Selection& other);

    BufferIterator begin() const;
    BufferIterator end() const;

    const BufferIterator& first() const { return m_first; }
    const BufferIterator& last()  const { return m_last; }

    void merge_with(const Selection& selection);

    BufferString capture(size_t index) const;
    const CaptureList& captures() const { return m_captures; }

private:
    BufferIterator m_first;
    BufferIterator m_last;

    CaptureList m_captures;

    void on_modification(const Modification& modification);

    void register_with_buffer();
    void unregister_with_buffer();

    void check_invariant();
};

typedef std::vector<Selection> SelectionList;

}

#endif // selection_hh_INCLUDED

