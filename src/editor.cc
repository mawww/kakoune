#include "editor.hh"

#include "exception.hh"
#include "register.hh"
#include "register_manager.hh"
#include "utf8_iterator.hh"
#include "utils.hh"

#include <array>

namespace Kakoune
{

Editor::Editor(Buffer& buffer)
    : m_buffer(&buffer),
      m_edition_level(0),
      m_selections(buffer)
{
    m_selections.push_back(Selection({}, {}));
    m_main_sel = 0;
}

static void avoid_eol(const Buffer& buffer, BufferCoord& coord)
{
    const auto column = coord.column;
    if (column != 0 and column == buffer.line_length(coord.line) - 1)
        coord = buffer.char_prev(coord);
}

static void avoid_eol(const Buffer& buffer, Range& sel)
{
    avoid_eol(buffer, sel.first());
    avoid_eol(buffer, sel.last());
}

void Editor::erase()
{
    scoped_edition edition(*this);
    for (auto& sel : m_selections)
    {
        Kakoune::erase(*m_buffer, sel);
        avoid_eol(*m_buffer, sel);
    }
}

static BufferCoord prepare_insert(Buffer& buffer, const Selection& sel,
                                  InsertMode mode)
{
    switch (mode)
    {
    case InsertMode::Insert:
        return sel.min();
    case InsertMode::Replace:
    {
        BufferCoord pos = sel.min();
        Kakoune::erase(buffer, sel);
        return pos;
    }
    case InsertMode::Append:
    {
        // special case for end of lines, append to current line instead
        auto pos = std::max(sel.first(), sel.last());
        if (pos.column == buffer.line_length(pos.line) - 1)
            return pos;
        else
            return buffer.char_next(pos);
    }
    case InsertMode::InsertAtLineBegin:
        return sel.min().line;
    case InsertMode::AppendAtLineEnd:
        return buffer.char_prev(sel.max().line+1);
    case InsertMode::InsertAtNextLineBegin:
        return sel.max().line+1;
    case InsertMode::OpenLineBelow:
    case InsertMode::OpenLineAbove:
    {
        auto line = mode == InsertMode::OpenLineAbove ?
            sel.min().line : sel.max().line + 1;
        buffer.insert(line, "\n");
        return line;
    }

    }
    kak_assert(false);
    return {};
}

void Editor::insert(const String& str, InsertMode mode)
{
    scoped_edition edition(*this);

    for (auto& sel : m_selections)
    {
        BufferCoord pos = prepare_insert(*m_buffer, sel, mode);
        m_buffer->insert(pos, str);
        if (mode == InsertMode::Replace and not m_buffer->is_end(pos))
        {
            sel.first() = pos;
            sel.last()  = str.empty() ?
                 pos : m_buffer->char_advance(pos, str.char_length() - 1);
        }
        avoid_eol(*m_buffer, sel);
    }
    check_invariant();
}

void Editor::insert(const memoryview<String>& strings, InsertMode mode)
{
    scoped_edition edition(*this);
    if (strings.empty())
        return;

    for (size_t i = 0; i < selections().size(); ++i)
    {
        auto& sel = m_selections[i];
        BufferCoord pos = prepare_insert(*m_buffer, sel, mode);
        const String& str = strings[std::min(i, strings.size()-1)];
        m_buffer->insert(pos, str);
        if (mode == InsertMode::Replace and not m_buffer->is_end(pos))
        {
            sel.first() = pos;
            sel.last()  = str.empty() ?
                 pos : m_buffer->char_advance(pos, str.char_length() - 1);
        }
        avoid_eol(*m_buffer, sel);
    }
    check_invariant();
}

std::vector<String> Editor::selections_content() const
{
    std::vector<String> contents;
    for (auto& sel : m_selections)
        contents.push_back(m_buffer->string(sel.min(), m_buffer->char_next(sel.max())));
    return contents;
}

static bool compare_selections(const Selection& lhs, const Selection& rhs)
{
    return lhs.min() < rhs.min();
}

template<typename OverlapsFunc>
void merge_overlapping(SelectionList& selections, size_t& main_selection,
                       OverlapsFunc overlaps)
{
    kak_assert(std::is_sorted(selections.begin(), selections.end(), compare_selections));
    for (size_t i = 0; i+1 < selections.size() and selections.size() > 1;)
    {
        if (overlaps(selections[i], selections[i+1]))
        {
            selections[i].merge_with(selections[i+1]);
            selections.erase(selections.begin() + i + 1);
            if (i + 1 <= main_selection)
                --main_selection;
        }
        else
           ++i;
    }
}

void sort_and_merge_overlapping(SelectionList& selections, size_t& main_selection)
{
    if (selections.size() == 1)
        return;

    const auto& main = selections[main_selection];
    const auto main_begin = main.min();
    main_selection = std::count_if(selections.begin(), selections.end(),
                                   [&](const Selection& sel) {
                                       auto begin = sel.min();
                                       if (begin == main_begin)
                                           return &sel < &main;
                                       else
                                           return begin < main_begin;
                                   });
    std::stable_sort(selections.begin(), selections.end(), compare_selections);

    merge_overlapping(selections, main_selection, overlaps);
}

void Editor::move_selections(CharCount offset, SelectMode mode)
{
    kak_assert(mode == SelectMode::Replace or mode == SelectMode::Extend);
    for (auto& sel : m_selections)
    {
        auto last = sel.last();
        last = clamp<BufferCoord>(m_buffer->char_advance(last, offset),
                                  last.line, m_buffer->char_prev(last.line+1));
        sel.first() = mode == SelectMode::Extend ? sel.first() : last;
        sel.last()  = last;
        avoid_eol(*m_buffer, sel);
    }
    sort_and_merge_overlapping(m_selections, m_main_sel);
}

void Editor::move_selections(LineCount offset, SelectMode mode)
{
    kak_assert(mode == SelectMode::Replace or mode == SelectMode::Extend);
    for (auto& sel : m_selections)
    {
        CharCount column = m_buffer->char_distance(sel.last().line, sel.last());
        auto line = clamp(sel.last().line + offset, 0_line, m_buffer->line_count()-1);
        column = std::min(column, m_buffer->line_content(line).char_length()-1);
        BufferCoord last = m_buffer->char_advance(line, column);
        sel.first() = mode == SelectMode::Extend ? sel.first() : last;
        sel.last()  = last;
        avoid_eol(*m_buffer, sel);
    }
    sort_and_merge_overlapping(m_selections, m_main_sel);
}

void Editor::clear_selections()
{
    auto& sel = m_selections[m_main_sel];
    auto& pos = sel.last();

    if (pos.column != 0 and pos.column == m_buffer->line_length(pos.line) - 1)
        pos = m_buffer->char_prev(pos);
    sel.first() = pos;

    m_selections.erase(m_selections.begin(), m_selections.begin() + m_main_sel);
    m_selections.erase(m_selections.begin() + 1, m_selections.end());
    m_main_sel = 0;
    check_invariant();
}

void Editor::flip_selections()
{
    for (auto& sel : m_selections)
        std::swap(sel.first(), sel.last());
    check_invariant();
}

void Editor::keep_selection(int index)
{
    if (index < m_selections.size())
    {
        size_t real_index = (index + m_main_sel + 1) % m_selections.size();
        m_selections = SelectionList{ std::move(m_selections[real_index]) };
        m_main_sel = 0;
    }
    check_invariant();
}

void Editor::remove_selection(int index)
{
    if (m_selections.size() > 1 and index < m_selections.size())
    {
        size_t real_index = (index + m_main_sel + 1) % m_selections.size();
        m_selections.erase(m_selections.begin() + real_index);
        if (real_index <= m_main_sel)
            --m_main_sel;
    }
    check_invariant();
}

void Editor::select(const Selection& selection, SelectMode mode)
{
    if (mode == SelectMode::Replace)
    {
        m_selections = SelectionList{ selection };
        m_main_sel = 0;
    }
    else if (mode == SelectMode::Extend)
    {
        m_selections[m_main_sel].merge_with(selection);
        m_selections = SelectionList{ std::move(m_selections[m_main_sel]) };
        m_main_sel = 0;
    }
    else if (mode == SelectMode::Append)
    {
        m_main_sel = m_selections.size();
        m_selections.push_back(selection);
        sort_and_merge_overlapping(m_selections, m_main_sel);
    }
    else
        kak_assert(false);
    check_invariant();
}

void Editor::select(SelectionList selections)
{
    if (selections.empty())
        throw runtime_error("no selections");
    m_selections = std::move(selections);
    m_main_sel = m_selections.size() - 1;
    check_invariant();
}

void Editor::select(const Selector& selector, SelectMode mode)
{
    if (mode == SelectMode::Append)
    {
        auto& sel = m_selections[m_main_sel];
        auto  res = selector(*m_buffer, sel);
        if (res.captures().empty())
            res.captures() = sel.captures();
        m_main_sel = m_selections.size();
        m_selections.push_back(res);
    }
    else if (mode == SelectMode::ReplaceMain)
    {
        auto& sel = m_selections[m_main_sel];
        auto  res = selector(*m_buffer, sel);
        sel.first() = res.first();
        sel.last()  = res.last();
        if (not res.captures().empty())
            sel.captures() = std::move(res.captures());
    }
    else
    {
        for (auto& sel : m_selections)
        {
            auto res = selector(*m_buffer, sel);
            if (mode == SelectMode::Extend)
                sel.merge_with(res);
            else
            {
                sel.first() = res.first();
                sel.last()  = res.last();
            }
            if (not res.captures().empty())
                sel.captures() = std::move(res.captures());
        }
    }
    sort_and_merge_overlapping(m_selections, m_main_sel);
    check_invariant();
}

struct nothing_selected : public runtime_error
{
    nothing_selected() : runtime_error("nothing was selected") {}
};

void Editor::multi_select(const MultiSelector& selector)
{
    SelectionList new_selections;
    for (auto& sel : m_selections)
    {
        SelectionList res = selector(*m_buffer, sel);
        new_selections.reserve(new_selections.size() + res.size());
        for (auto& new_sel : res)
        {
            // preserve captures when selectors captures nothing.
            if (new_sel.captures().empty())
                new_selections.emplace_back(new_sel.first(), new_sel.last(),
                                            sel.captures());
            else
                new_selections.push_back(std::move(new_sel));
        }
    }
    if (new_selections.empty())
        throw nothing_selected();
    m_main_sel = new_selections.size() - 1;
    sort_and_merge_overlapping(new_selections, m_main_sel);
    m_selections = std::move(new_selections);
    check_invariant();
}

class ModifiedRangesListener : public BufferChangeListener_AutoRegister
{
public:
    ModifiedRangesListener(Buffer& buffer)
        : BufferChangeListener_AutoRegister(buffer) {}

    void on_insert(const Buffer& buffer, const BufferCoord& begin, const BufferCoord& end)
    {
        m_ranges.update_insert(buffer, begin, end);
        auto it = std::upper_bound(m_ranges.begin(), m_ranges.end(), begin,
                                   [](const BufferCoord& c, const Selection& sel)
                                   { return c < sel.min(); });
        m_ranges.emplace(it, begin, buffer.char_prev(end));
    }

    void on_erase(const Buffer& buffer, const BufferCoord& begin, const BufferCoord& end)
    {
        m_ranges.update_erase(buffer, begin, end);
        auto pos = std::min(begin, buffer.back_coord());
        auto it = std::upper_bound(m_ranges.begin(), m_ranges.end(), pos,
                                   [](const BufferCoord& c, const Selection& sel)
                                   { return c < sel.min(); });
        m_ranges.emplace(it, pos, pos);
    }
    SelectionList& ranges() { return m_ranges; }

private:
    SelectionList m_ranges;
};

inline bool touches(const Buffer& buffer, const Range& lhs, const Range& rhs)
{
    return lhs.min() <= rhs.min() ? buffer.char_next(lhs.max()) >= rhs.min()
                                  : lhs.min() <= buffer.char_next(rhs.max());
}

bool Editor::undo()
{
    using namespace std::placeholders;
    ModifiedRangesListener listener(buffer());
    bool res = m_buffer->undo();
    if (res and not listener.ranges().empty())
    {
        m_selections = std::move(listener.ranges());
        m_main_sel = m_selections.size() - 1;
        merge_overlapping(m_selections, m_main_sel, std::bind(touches, std::ref(buffer()), _1, _2));
    }
    check_invariant();
    return res;
}

bool Editor::redo()
{
    using namespace std::placeholders;
    ModifiedRangesListener listener(buffer());
    bool res = m_buffer->redo();
    if (res and not listener.ranges().empty())
    {
        m_selections = std::move(listener.ranges());
        m_main_sel = m_selections.size() - 1;
        merge_overlapping(m_selections, m_main_sel, std::bind(touches, std::ref(buffer()), _1, _2));
    }
    check_invariant();
    return res;
}

void Editor::check_invariant() const
{
#ifdef KAK_DEBUG
    kak_assert(not m_selections.empty());
    kak_assert(m_main_sel < m_selections.size());
    m_selections.check_invariant();
    buffer().check_invariant();
    kak_assert(std::is_sorted(m_selections.begin(), m_selections.end(), compare_selections));
#endif
}

void Editor::begin_edition()
{
    ++m_edition_level;
}

void Editor::end_edition()
{
    kak_assert(m_edition_level > 0);
    if (m_edition_level == 1)
        m_buffer->commit_undo_group();

    --m_edition_level;
}

IncrementalInserter::IncrementalInserter(Editor& editor, InsertMode mode)
    : m_editor(editor), m_edition(editor), m_mode(mode)
{
    Buffer& buffer = *editor.m_buffer;

    for (auto& sel : m_editor.m_selections)
    {
        BufferCoord first, last;
        switch (mode)
        {
        case InsertMode::Insert:  first = sel.max(); last = sel.min(); break;
        case InsertMode::Replace:
        {
            Kakoune::erase(buffer, sel);
            first = last = sel.min();
            break;
        }
        case InsertMode::Append:
        {
            first = sel.min();
            last  = sel.max();
            // special case for end of lines, append to current line instead
            if (last.column != buffer.line_length(last.line) - 1)
                last = buffer.char_next(last);
            break;
        }

        case InsertMode::OpenLineBelow:
        case InsertMode::AppendAtLineEnd:
            first = buffer.char_prev(sel.max().line+1);
            last  = first;
            break;

        case InsertMode::OpenLineAbove:
        case InsertMode::InsertAtLineBegin:
            first = sel.min().line;
            if (mode == InsertMode::OpenLineAbove)
                first = buffer.char_prev(first);
            else
            {
                auto first_non_blank = buffer.iterator_at(first);
                while (*first_non_blank == ' ' or *first_non_blank == '\t')
                    ++first_non_blank;
                if (*first_non_blank != '\n')
                    first = first_non_blank.coord();
            }
            last = first;
            break;
        case InsertMode::InsertAtNextLineBegin:
             kak_assert(false); // not implemented
             break;
        }
        if (buffer.is_end(first))
           first = buffer.char_prev(first);
        if (buffer.is_end(last))
           last = buffer.char_prev(last);
        sel.first() = first;
        sel.last()  = last;
    }
    if (mode == InsertMode::OpenLineBelow or mode == InsertMode::OpenLineAbove)
    {
        insert("\n");
        if (mode == InsertMode::OpenLineAbove)
        {
            for (auto& sel : m_editor.m_selections)
            {
                // special case, the --first line above did nothing, so we need to compensate now
                if (sel.first() == buffer.char_next({0,0}))
                    sel.first() = sel.last() = BufferCoord{0,0};
            }
        }
    }
    sort_and_merge_overlapping(editor.m_selections, editor.m_main_sel);
    editor.check_invariant();
}

IncrementalInserter::~IncrementalInserter()
{
    for (auto& sel : m_editor.m_selections)
    {
        if (m_mode == InsertMode::Append and sel.last().column > 0)
            sel.last() = m_editor.buffer().char_prev(sel.last());
        avoid_eol(m_editor.buffer(), sel);
    }
}

void IncrementalInserter::insert(String content)
{
    Buffer& buffer = m_editor.buffer();
    for (auto& sel : m_editor.m_selections)
    {
        m_editor.filters()(buffer, sel, content);
        buffer.insert(sel.last(), content);
    }
}

void IncrementalInserter::insert(const memoryview<String>& strings)
{
    for (size_t i = 0; i < m_editor.m_selections.size(); ++i)
    {
        size_t index = std::min(i, strings.size()-1);
        m_editor.buffer().insert(m_editor.m_selections[i].last(),
                                 strings[index]);
    }
}

void IncrementalInserter::erase()
{
    for (auto& sel : m_editor.m_selections)
    {
        BufferCoord pos = sel.last();
        m_editor.buffer().erase(m_editor.buffer().char_prev(pos), pos);
    }
}

void IncrementalInserter::move_cursors(CharCount move)
{
    m_editor.move_selections(move, SelectMode::Replace);
}

void IncrementalInserter::move_cursors(LineCount move)
{
    m_editor.move_selections(move, SelectMode::Replace);
}

}
