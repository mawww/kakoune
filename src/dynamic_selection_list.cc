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
    SelectionList::check_invariant();
    const Buffer& buffer = registry();
    for (size_t i = 0; i < size(); ++i)
    {
        auto& sel = (*this)[i];
        kak_assert(buffer.is_valid(sel.first()));
        kak_assert(buffer.is_valid(sel.last()));
        kak_assert(not buffer.is_end(sel.first()));
        kak_assert(not buffer.is_end(sel.last()));
        kak_assert(utf8::is_character_start(buffer.iterator_at(sel.first())));
        kak_assert(utf8::is_character_start(buffer.iterator_at(sel.last())));
    }
#endif
}

void DynamicSelectionList::on_insert(const Buffer& buffer, const BufferCoord& begin, const BufferCoord& end)
{
    update_insert(buffer, begin, end);
}

void DynamicSelectionList::on_erase(const Buffer& buffer, const BufferCoord& begin, const BufferCoord& end)
{
    update_erase(buffer, begin, end);
}

}
