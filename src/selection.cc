#include "selection.hh"

#include "utf8.hh"

namespace Kakoune
{

void Range::merge_with(const Range& range)
{
    m_last = range.m_last;
    if (m_first < m_last)
        m_first = std::min(m_first, range.m_first);
    if (m_first > m_last)
        m_first = std::max(m_first, range.m_first);
}

namespace
{

template<template <bool, bool> class UpdateFunc>
void on_buffer_change(const Buffer& buffer, SelectionList& sels,
                      const BufferCoord& begin, const BufferCoord& end, LineCount end_line)
{
    auto update_beg = std::lower_bound(sels.begin(), sels.end(), begin,
                                       [](const Selection& s, const BufferCoord& c) { return std::max(s.first().coord(), s.last().coord()) < c; });
    auto update_only_line_beg = std::upper_bound(sels.begin(), sels.end(), end_line,
                                                 [](LineCount l, const Selection& s) { return l < std::min(s.first().coord(), s.last().coord()).line; });

    if (update_beg != update_only_line_beg)
    {
        // for the first one, we are not sure if min < begin
        UpdateFunc<false, false>{}(buffer, update_beg->first(), begin, end);
        UpdateFunc<false, false>{}(buffer, update_beg->last(), begin, end);
    }
    for (auto it = update_beg+1; it < update_only_line_beg; ++it)
    {
        UpdateFunc<false, true>{}(buffer, it->first(), begin, end);
        UpdateFunc<false, true>{}(buffer, it->last(), begin, end);
    }
    if (end.line > begin.line)
    {
        for (auto it = update_only_line_beg; it != sels.end(); ++it)
        {
            UpdateFunc<true, true>{}(buffer, it->first(), begin, end);
            UpdateFunc<true, true>{}(buffer, it->last(), begin, end);
        }
    }
}

template<bool assume_different_line, bool assume_greater_than_begin>
struct UpdateInsert
{
    void operator()(const Buffer& buffer, BufferIterator& it,
                    const BufferCoord& begin, const BufferCoord& end) const
    {
        auto coord = it.coord();
        if (assume_different_line)
            kak_assert(begin.line < coord.line);
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
    void operator()(const Buffer& buffer, BufferIterator& it,
                    const BufferCoord& begin, const BufferCoord& end) const
    {
        auto coord = it.coord();
        if (not assume_greater_than_begin and coord < begin)
            return;
        if (assume_different_line)
            kak_assert(end.line < coord.line);
        if (not assume_different_line and coord <= end)
            coord = buffer.clamp(begin);
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

void SelectionList::update_insert(const Buffer& buffer, const BufferCoord& begin, const BufferCoord& end)
{
    on_buffer_change<UpdateInsert>(buffer, *this, begin, end, begin.line);
}

void SelectionList::update_erase(const Buffer& buffer, const BufferCoord& begin, const BufferCoord& end)
{
    on_buffer_change<UpdateErase>(buffer, *this, begin, end, end.line);
}

void SelectionList::check_invariant() const
{
    for (size_t i = 0; i+1 < size(); ++ i)
        kak_assert((*this)[i].min() <= (*this)[i+1].min());
}

}
