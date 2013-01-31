#include "dynamic_selection_list.hh"

namespace Kakoune
{

DynamicSelectionList::DynamicSelectionList(const Buffer& buffer,
                                           SelectionList selections)
    : m_buffer(&buffer), SelectionList(std::move(selections))
{
    m_buffer->change_listeners().insert(this);
    check_invariant();
}

DynamicSelectionList::~DynamicSelectionList()
{
    m_buffer->change_listeners().erase(this);
}

DynamicSelectionList::DynamicSelectionList(const DynamicSelectionList& other)
    : SelectionList(other), m_buffer(other.m_buffer)
{
    m_buffer->change_listeners().insert(this);
}

DynamicSelectionList& DynamicSelectionList::operator=(const DynamicSelectionList& other)
{
    SelectionList::operator=((const SelectionList&)other);
    if (m_buffer != other.m_buffer)
    {
        m_buffer->change_listeners().erase(this);
        m_buffer = other.m_buffer;
        m_buffer->change_listeners().insert(this);
    }
    check_invariant();
    return *this;
}

DynamicSelectionList::DynamicSelectionList(DynamicSelectionList&& other)
    : SelectionList(std::move(other)), m_buffer(other.m_buffer)
{
    m_buffer->change_listeners().insert(this);
}

DynamicSelectionList& DynamicSelectionList::operator=(DynamicSelectionList&& other)
{
    SelectionList::operator=(std::move(other));
    if (m_buffer != other.m_buffer)
    {
        m_buffer->change_listeners().erase(this);
        m_buffer = other.m_buffer;
        m_buffer->change_listeners().insert(this);
    }
    check_invariant();
    return *this;
}

DynamicSelectionList& DynamicSelectionList::operator=(SelectionList selections)
{
    SelectionList::operator=(std::move(selections));
    check_invariant();
    return *this;
}

void DynamicSelectionList::check_invariant() const
{
    for (auto& sel : *this)
    {
        assert(m_buffer == &sel.buffer());
        sel.check_invariant();
    }
}

void DynamicSelectionList::on_insert(const BufferIterator& begin, const BufferIterator& end)
{
    for (auto& sel : *this)
    {
        sel.first().on_insert(begin.coord(), end.coord());
        sel.last().on_insert(begin.coord(), end.coord());
    }
}

void DynamicSelectionList::on_erase(const BufferIterator& begin, const BufferIterator& end)
{
    for (auto& sel : *this)
    {
        sel.first().on_erase(begin.coord(), end.coord());
        sel.last().on_erase(begin.coord(), end.coord());
    }
}

}
