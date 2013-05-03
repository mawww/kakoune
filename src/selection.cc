#include "selection.hh"

#include "utf8.hh"

namespace Kakoune
{

void Range::merge_with(const Range& range)
{
    if (m_first < m_last)
        m_first = std::min(m_first, range.m_first);
    if (m_first > m_last)
        m_first = std::max(m_first, range.m_first);
    m_last = range.m_last;
}

BufferIterator Range::begin() const
{
    return std::min(m_first, m_last);
}

BufferIterator Range::end() const
{
    return utf8::next(std::max(m_first, m_last));
}

String Range::content() const
{
    return m_first.buffer().string(begin(), end());
}

void Range::check_invariant() const
{
#ifdef KAK_DEBUG
    kak_assert(m_first.is_valid());
    kak_assert(m_last.is_valid());
    kak_assert(utf8::is_character_start(m_first));
    kak_assert(utf8::is_character_start(m_last));
#endif
}

static void avoid_eol(BufferIterator& it)
{
    const auto column = it.column();
    if (column != 0 and column == it.buffer().line_length(it.line()) - 1)
        it = utf8::previous(it);
}

void Selection::avoid_eol()
{
    Kakoune::avoid_eol(first());
    Kakoune::avoid_eol(last());
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
    void operator()(BufferIterator& it,
                    const BufferCoord& begin, const BufferCoord& end) const
    {
        BufferCoord coord = it.coord();
        if (not assume_greater_than_begin and coord < begin)
            return;
        if (assume_different_line)
            kak_assert(end.line < coord.line);
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

void SelectionList::update_insert(const BufferCoord& begin, const BufferCoord& end)
{
    on_buffer_change<UpdateInsert>(*this, begin, end, begin.line);
}

void SelectionList::update_erase(const BufferCoord& begin, const BufferCoord& end)
{
    on_buffer_change<UpdateErase>(*this, begin, end, end.line);
}

void SelectionList::check_invariant() const
{
#ifdef KAK_DEBUG
    for (size_t i = 0; i < size(); ++i)
    {
        auto& sel = (*this)[i];
        sel.check_invariant();
        if (i+1 < size())
            kak_assert(sel.begin() <= (*this)[i+1].begin());
    }
#endif
}

}
