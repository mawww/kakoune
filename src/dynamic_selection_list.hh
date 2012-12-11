#ifndef dynamic_selection_list_hh_INCLUDED
#define dynamic_selection_list_hh_INCLUDED

#include "selection.hh"

namespace Kakoune
{

class DynamicSelectionList : public BufferChangeListener
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

    size_t size()  const { return m_selections.size(); }
    bool   empty() const { return m_selections.empty(); }

    void clear() { m_selections.clear(); }
    iterator erase(iterator it) { return m_selections.erase(it); }

    void push_back(Selection selection)
    {
        assert(&selection.buffer() == m_buffer);
        m_selections.push_back(std::move(selection));
    }

    void reset(SelectionList selections);

    iterator begin() { return m_selections.begin(); }
    iterator end()   { return m_selections.end(); }
    const_iterator begin() const { return m_selections.begin(); }
    const_iterator end()   const { return m_selections.end(); }

    Selection& front() { return m_selections.front(); }
    Selection& back()  { return m_selections.back(); }
    const Selection& front() const { return m_selections.front(); }
    const Selection& back()  const { return m_selections.back(); }

    Selection& operator[](size_t index) { return m_selections[index]; }
    const Selection& operator[](size_t index) const { return m_selections[index]; }

    operator const SelectionList&() const { return m_selections; }

    const Buffer& buffer() const { return *m_buffer; }

private:
    void on_insert(const BufferIterator& begin,
                   const BufferIterator& end) override;
    void on_erase(const BufferIterator& begin,
                  const BufferIterator& end) override;

    const Buffer* m_buffer;
    SelectionList m_selections;
};

};

#endif // dynamic_selection_list_hh_INCLUDED

