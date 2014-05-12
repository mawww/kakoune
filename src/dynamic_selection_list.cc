#include "dynamic_selection_list.hh"

namespace Kakoune
{

DynamicSelectionList::DynamicSelectionList(SelectionList selections)
    : SelectionList(std::move(selections)),
      BufferChangeListener_AutoRegister(const_cast<Buffer&>(buffer()))
{
    check_invariant();
}

DynamicSelectionList& DynamicSelectionList::operator=(SelectionList selections)
{
    SelectionList::operator=(std::move(selections));
    check_invariant();
    return *this;
}

void DynamicSelectionList::on_insert(const Buffer& buffer, ByteCoord begin, ByteCoord end, bool at_end)
{
    update_insert(begin, end, at_end);
    set_timestamp(buffer.timestamp());
}

void DynamicSelectionList::on_erase(const Buffer& buffer, ByteCoord begin, ByteCoord end, bool at_end)
{
    update_erase(begin, end, at_end);
    set_timestamp(buffer.timestamp());
}

}
