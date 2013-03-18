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

namespace
{

template<template <bool, bool> class UpdateFunc>
void on_buffer_change(SelectionList& sels, const BufferCoord& begin, const BufferCoord& end, LineCount end_line)
{
    auto update_beg = std::lower_bound(sels.begin(), sels.end(), begin,
                                       [](const Selection& s, const BufferCoord& c) { return std::max(s.first(), s.last()).coord() < c; });
    auto update_only_line_beg = std::upper_bound(sels.begin(), sels.end(), end_line,
                                                 [](LineCount l, const Selection& s) { return l < std::min(s.first(), s.last()).line(); });

    if (update_beg != update_only_line_beg)
    {
        // for the first one, we are not sure if min < begin
        UpdateFunc<false, false>{}(update_beg->first(), begin, end);
        UpdateFunc<false, false>{}(update_beg->last(), begin, end);
    }
    for (auto it = update_beg+1; it < update_only_line_beg; ++it)
    {
        UpdateFunc<false, true>{}(it->first(), begin, end);
        UpdateFunc<false, true>{}(it->last(), begin, end);
    }
    if (end.line > begin.line)
    {
        for (auto it = update_only_line_beg; it != sels.end(); ++it)
        {
            UpdateFunc<true, true>{}(it->first(), begin, end);
            UpdateFunc<true, true>{}(it->last(), begin, end);
        }
    }
}

template<bool assume_different_line, bool assume_greater_than_begin>
struct UpdateInsert
{
    void operator()(BufferIterator& it,
                    const BufferCoord& begin, const BufferCoord& end) const
    {
        BufferCoord coord = it.coord();
        if (assume_different_line)
            assert(begin.line < coord.line);
        if (not assume_greater_than_begin and coord < begin)
            return;
        if (not assume_different_line and begin.line == coord.line)
            coord.column = end.column + coord.column - begin.column;

        coord.line += end.line - begin.line;
        it = coord;
    }
};

template<bool assume_different_line, bool assume_greater_than_begin>
struct UpdateErase
{
    void operator()(BufferIterator& it,
                    const BufferCoord& begin, const BufferCoord& end) const
    {
        BufferCoord coord = it.coord();
        if (not assume_greater_than_begin and coord < begin)
            return;
        if (assume_different_line)
            assert(end.line < coord.line);
        if (not assume_different_line and coord <= end)
            coord = it.buffer().clamp(begin);
        else
        {
            if (not assume_different_line and end.line == coord.line)
            {
                coord.line = begin.line;
                coord.column = begin.column + coord.column - end.column;
            }
            else
                coord.line -= end.line - begin.line;
        }
        it = coord;
    }
};

}

void DynamicSelectionList::on_insert(const BufferIterator& begin, const BufferIterator& end)
{
    on_buffer_change<UpdateInsert>(*this, begin.coord(), end.coord(), begin.line());
}

void DynamicSelectionList::on_erase(const BufferIterator& begin, const BufferIterator& end)
{
    on_buffer_change<UpdateErase>(*this, begin.coord(), end.coord(), end.line());
}

}
