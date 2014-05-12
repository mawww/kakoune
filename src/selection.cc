#include "selection.hh"

#include "utf8.hh"

namespace Kakoune
{

void Selection::merge_with(const Selection& range)
{
    m_cursor = range.m_cursor;
    if (m_anchor < m_cursor)
        m_anchor = std::min(m_anchor, range.m_anchor);
    if (m_anchor > m_cursor)
        m_anchor = std::max(m_anchor, range.m_anchor);
}

namespace
{

template<template <bool, bool> class UpdateFunc>
void on_buffer_change(SelectionList& sels,
                      ByteCoord begin, ByteCoord end, bool at_end, LineCount end_line)
{
    auto update_beg = std::lower_bound(sels.begin(), sels.end(), begin,
                                       [](const Selection& s, ByteCoord c)
                                       { return s.max() < c; });
    auto update_only_line_beg = std::upper_bound(sels.begin(), sels.end(), end_line,
                                                 [](LineCount l, const Selection& s)
                                                 { return l < s.min().line; });

    if (update_beg != update_only_line_beg)
    {
        // for the first one, we are not sure if min < begin
        UpdateFunc<false, false>{}(update_beg->anchor(), begin, end, at_end);
        UpdateFunc<false, false>{}(update_beg->cursor(), begin, end, at_end);
    }
    for (auto it = update_beg+1; it < update_only_line_beg; ++it)
    {
        UpdateFunc<false, true>{}(it->anchor(), begin, end, at_end);
        UpdateFunc<false, true>{}(it->cursor(), begin, end, at_end);
    }
    if (end.line > begin.line)
    {
        for (auto it = update_only_line_beg; it != sels.end(); ++it)
        {
            UpdateFunc<true, true>{}(it->anchor(), begin, end, at_end);
            UpdateFunc<true, true>{}(it->cursor(), begin, end, at_end);
        }
    }
}

template<bool assume_different_line, bool assume_greater_than_begin>
struct UpdateInsert
{
    void operator()(ByteCoord& coord, ByteCoord begin, ByteCoord end,
                    bool at_end) const
    {
        if (assume_different_line)
            kak_assert(begin.line < coord.line);
        if (not assume_greater_than_begin and coord < begin)
            return;
        if (not assume_different_line and begin.line == coord.line)
            coord.column = end.column + coord.column - begin.column;

        coord.line += end.line - begin.line;
    }
};

template<bool assume_different_line, bool assume_greater_than_begin>
struct UpdateErase
{
    void operator()(ByteCoord& coord, ByteCoord begin, ByteCoord end,
                    bool at_end) const
    {
        if (not assume_greater_than_begin and coord < begin)
            return;
        if (assume_different_line)
            kak_assert(end.line < coord.line);
        if (not assume_different_line and coord <= end)
        {
            if (not at_end)
                coord = begin;
            else
                coord = begin.column ? ByteCoord{begin.line, begin.column-1}
                                     : ByteCoord{begin.line - 1};
        }
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
    }
};

}

void SelectionList::update_insert(ByteCoord begin, ByteCoord end, bool at_end)
{
    on_buffer_change<UpdateInsert>(*this, begin, end, at_end, begin.line);
}

void SelectionList::update_erase(ByteCoord begin, ByteCoord end, bool at_end)
{
    on_buffer_change<UpdateErase>(*this, begin, end, at_end, end.line);
}

void SelectionList::check_invariant() const
{
    kak_assert(size() > 0);
    kak_assert(m_main < size());
    for (size_t i = 0; i+1 < size(); ++ i)
        kak_assert((*this)[i].min() <= (*this)[i+1].min());
}

}
