#include "dynamic_selection_list.hh"

namespace Kakoune
{

DynamicSelectionList::DynamicSelectionList(const Buffer& buffer,
                                           SelectionList selections)
    : m_buffer(&buffer), m_selections(std::move(selections))
{
    m_buffer->add_change_listener(*this);
}

DynamicSelectionList::~DynamicSelectionList()
{
    m_buffer->remove_change_listener(*this);
}

DynamicSelectionList::DynamicSelectionList(const DynamicSelectionList& other)
    : m_selections(other.m_selections), m_buffer(other.m_buffer)
{
    m_buffer->add_change_listener(*this);
}

DynamicSelectionList& DynamicSelectionList::operator=(const DynamicSelectionList& other)
{
    m_selections = other.m_selections;
    if (m_buffer != other.m_buffer)
    {
        m_buffer->remove_change_listener(*this);
        m_buffer = other.m_buffer;
        m_buffer->add_change_listener(*this);
    }
    return *this;
}

DynamicSelectionList::DynamicSelectionList(DynamicSelectionList&& other)
    : m_selections(std::move(other.m_selections)), m_buffer(other.m_buffer)
{
    m_buffer->add_change_listener(*this);
}

DynamicSelectionList& DynamicSelectionList::operator=(DynamicSelectionList&& other)
{
    m_selections = std::move(other.m_selections);
    if (m_buffer != other.m_buffer)
    {
        m_buffer->remove_change_listener(*this);
        m_buffer = other.m_buffer;
        m_buffer->add_change_listener(*this);
    }
    return *this;
}

void DynamicSelectionList::reset(SelectionList selections)
{
    for (auto& sel : selections)
        assert(&sel.buffer() == m_buffer);
     m_selections = std::move(selections);
}

void DynamicSelectionList::on_insert(const BufferIterator& begin, const BufferIterator& end)
{
    for (auto& sel : m_selections)
    {
        sel.first().on_insert(begin.coord(), end.coord());
        sel.last().on_insert(begin.coord(), end.coord());
        sel.check_invariant();
    }
}

void DynamicSelectionList::on_erase(const BufferIterator& begin, const BufferIterator& end)
{
    for (auto& sel : m_selections)
    {
        sel.first().on_erase(begin.coord(), end.coord());
        sel.last().on_erase(begin.coord(), end.coord());
        sel.check_invariant();
    }
}

}
