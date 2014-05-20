#include "selection.hh"

#include "utf8.hh"
#include "modification.hh"

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
void on_buffer_change(std::vector<Selection>& sels,
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
        kak_assert(coord.line >= 0 and coord.column >= 0);
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
            if (not at_end or begin == ByteCoord{0,0})
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
        kak_assert(coord.line >= 0 and coord.column >= 0);
    }
};

}

SelectionList::SelectionList(const Buffer& buffer, Selection s, size_t timestamp)
    : m_buffer(&buffer), m_selections({ s }), m_timestamp(timestamp)
{
    check_invariant();
}

SelectionList::SelectionList(const Buffer& buffer, Selection s)
    : SelectionList(buffer, s, buffer.timestamp())
{}

SelectionList::SelectionList(const Buffer& buffer, std::vector<Selection> s, size_t timestamp)
    : m_buffer(&buffer), m_selections(std::move(s)), m_timestamp(timestamp)
{
    kak_assert(size() > 0);
    check_invariant();
}

SelectionList::SelectionList(const Buffer& buffer, std::vector<Selection> s)
    : SelectionList(buffer, std::move(s), buffer.timestamp())
{}

void update_insert(std::vector<Selection>& sels, ByteCoord begin, ByteCoord end, bool at_end)
{
    on_buffer_change<UpdateInsert>(sels, begin, end, at_end, begin.line);
}

void update_erase(std::vector<Selection>& sels, ByteCoord begin, ByteCoord end, bool at_end)
{
    on_buffer_change<UpdateErase>(sels, begin, end, at_end, end.line);
}

static ByteCoord update_pos(memoryview<Modification> modifs, ByteCoord pos)
{
    auto modif_it = std::upper_bound(modifs.begin(), modifs.end(), pos,
                                     [](const ByteCoord& c, const Modification& m)
                                     { return c < m.old_coord; });
    if (modif_it != modifs.begin())
    {
        auto& prev = *(modif_it-1);
        return prev.get_new_coord(pos);
    }
    return pos;
}

void SelectionList::update()
{
    if (m_timestamp == m_buffer->timestamp())
        return;

    auto modifs = compute_modifications(*m_buffer, m_timestamp);
    for (auto& sel : m_selections)
    {
        auto anchor = update_pos(modifs, sel.anchor());
        kak_assert(m_buffer->is_valid(anchor));
        sel.anchor() = anchor;

        auto cursor = update_pos(modifs, sel.cursor());
        kak_assert(m_buffer->is_valid(cursor));
        sel.cursor() = cursor;
    }

    check_invariant();

    m_timestamp = m_buffer->timestamp();
}

void SelectionList::check_invariant() const
{
#ifdef KAK_DEBUG
    auto& buffer = this->buffer();
    kak_assert(size() > 0);
    kak_assert(m_main < size());
    for (size_t i = 0; i < size(); ++i)
    {
        auto& sel = (*this)[i];
        if (i+1 < size())
            kak_assert((*this)[i].min() <= (*this)[i+1].min());
        kak_assert(buffer.is_valid(sel.anchor()));
        kak_assert(buffer.is_valid(sel.cursor()));
        kak_assert(not buffer.is_end(sel.anchor()));
        kak_assert(not buffer.is_end(sel.cursor()));
        kak_assert(utf8::is_character_start(buffer.byte_at(sel.anchor())));
        kak_assert(utf8::is_character_start(buffer.byte_at(sel.cursor())));
    }
#endif
}

void SelectionList::sort_and_merge_overlapping()
{
    if (size() == 1)
        return;

    const auto& main = this->main();
    const auto main_begin = main.min();
    m_main = std::count_if(begin(), end(), [&](const Selection& sel) {
                               auto begin = sel.min();
                               if (begin == main_begin)
                                   return &sel < &main;
                               else
                                   return begin < main_begin;
                           });
    std::stable_sort(begin(), end(), compare_selections);
    merge_overlapping(overlaps);
}
namespace
{

inline void _avoid_eol(const Buffer& buffer, ByteCoord& coord)
{
    const auto column = coord.column;
    const auto& line = buffer[coord.line];
    if (column != 0 and column == line.length() - 1)
        coord.column = line.byte_count_to(line.char_length() - 2);
}


inline void _avoid_eol(const Buffer& buffer, Selection& sel)
{
    _avoid_eol(buffer, sel.anchor());
    _avoid_eol(buffer, sel.cursor());
}

}

void SelectionList::avoid_eol()
{
    update();
    for (auto& sel : m_selections)
        _avoid_eol(buffer(), sel);
}

}
