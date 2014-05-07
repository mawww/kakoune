#include "dynamic_selection_list.hh"

namespace Kakoune
{

DynamicSelectionList::DynamicSelectionList(Buffer& buffer,
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
        kak_assert(buffer.is_valid(sel.anchor()));
        kak_assert(buffer.is_valid(sel.cursor()));
        kak_assert(not buffer.is_end(sel.anchor()));
        kak_assert(not buffer.is_end(sel.cursor()));
        kak_assert(utf8::is_character_start(buffer.iterator_at(sel.anchor())));
        kak_assert(utf8::is_character_start(buffer.iterator_at(sel.cursor())));
    }
#endif
}

void DynamicSelectionList::on_insert(const Buffer& buffer, ByteCoord begin, ByteCoord end)
{
    update_insert(buffer, begin, end);
}

void DynamicSelectionList::on_erase(const Buffer& buffer, ByteCoord begin, ByteCoord end)
{
    update_erase(buffer, begin, end);
}

}
