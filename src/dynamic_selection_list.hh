#ifndef dynamic_selection_list_hh_INCLUDED
#define dynamic_selection_list_hh_INCLUDED

#include "selection.hh"

namespace Kakoune
{

class DynamicSelectionList : public SelectionList, public BufferChangeListener
{
public:
    using iterator = SelectionList::iterator;
    using const_iterator = SelectionList::const_iterator;

    DynamicSelectionList(const Buffer& buffer, SelectionList selections = {});
    ~DynamicSelectionList();

    DynamicSelectionList(const DynamicSelectionList& other);
    DynamicSelectionList& operator=(const DynamicSelectionList& other);
    DynamicSelectionList(DynamicSelectionList&& other);
    DynamicSelectionList& operator=(DynamicSelectionList&& other);

    DynamicSelectionList& operator=(SelectionList selections);
    void check_invariant() const;

    const Buffer& buffer() const { return *m_buffer; }

private:
    void on_insert(const BufferIterator& begin,
                   const BufferIterator& end) override;
    void on_erase(const BufferIterator& begin,
                  const BufferIterator& end) override;

    const Buffer* m_buffer;
};

};

#endif // dynamic_selection_list_hh_INCLUDED

