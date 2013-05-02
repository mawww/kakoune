#include "dynamic_selection_list.hh"

namespace Kakoune
{

DynamicSelectionList::DynamicSelectionList(const Buffer& buffer,
                                           SelectionList selections)
    : SelectionList(std::move(selections)),
      BufferChangeListener_AutoRegister(buffer)
{
    check_invariant();
}

DynamicSelectionList& DynamicSelectionList::operator=(SelectionList selections)
{
    SelectionList::operator=(std::move(selections));
    check_invariant();
    return *this;
}

void DynamicSelectionList::check_invariant() const
{
#ifdef KAK_DEBUG
    const Buffer* buf = &buffer();
    for (auto& sel : *this)
    {
        kak_assert(buf == &sel.buffer());
        sel.check_invariant();
    }
#endif
}

void DynamicSelectionList::on_insert(const BufferIterator& begin, const BufferIterator& end)
{
    update_insert(begin.coord(), end.coord());
}

void DynamicSelectionList::on_erase(const BufferIterator& begin, const BufferIterator& end)
{
    update_erase(begin.coord(), end.coord());
}

}
