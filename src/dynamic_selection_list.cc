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
#ifdef KAK_DEBUG
    for (auto& sel : *this)
    {
        assert(m_buffer == &sel.buffer());
        sel.check_invariant();
    }
#endif
}

static void update_insert(BufferIterator& it,
                          const BufferCoord& begin, const BufferCoord& end)
{
    BufferCoord coord = it.coord();
    if (coord < begin)
        return;

    if (begin.line == coord.line)
        coord.column = end.column + coord.column - begin.column;
    coord.line += end.line - begin.line;

    it = coord;
}

static void update_erase(BufferIterator& it,
                         const BufferCoord& begin, const BufferCoord& end)
{
    BufferCoord coord = it.coord();
    if (coord < begin)
        return;

    if (coord <= end)
        coord = it.buffer().clamp(begin);
    else
    {
        if (end.line == coord.line)
        {
            coord.line = begin.line;
            coord.column = begin.column + coord.column - end.column;
        }
        else
            coord.line -= end.line - begin.line;
    }
    it = coord;
}

void DynamicSelectionList::on_insert(const BufferIterator& begin, const BufferIterator& end)
{
    const BufferCoord begin_coord{begin.coord()};
    const BufferCoord end_coord{end.coord()};
    for (auto& sel : *this)
    {
        update_insert(sel.first(), begin_coord, end_coord);
        update_insert(sel.last(), begin_coord, end_coord);
    }
}

void DynamicSelectionList::on_erase(const BufferIterator& begin, const BufferIterator& end)
{
    const BufferCoord begin_coord{begin.coord()};
    const BufferCoord end_coord{end.coord()};
    for (auto& sel : *this)
    {
        update_erase(sel.first(), begin_coord, end_coord);
        update_erase(sel.last(), begin_coord, end_coord);
    }
}

}
