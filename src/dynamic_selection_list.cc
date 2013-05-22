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
    if (empty())
        return;
    const Buffer* buf = &buffer();
    kak_assert(&front().buffer() == buf);
    SelectionList::check_invariant();
#endif
}

void DynamicSelectionList::on_insert(const BufferCoord& begin, const BufferCoord& end)
{
    update_insert(begin, end);
}

void DynamicSelectionList::on_erase(const BufferCoord& begin, const BufferCoord& end)
{
    update_erase(begin, end);
}

}
