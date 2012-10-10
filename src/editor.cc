#include "editor.hh"

#include "exception.hh"
#include "utils.hh"
#include "register.hh"
#include "register_manager.hh"

#include "utf8_iterator.hh"

#include <array>

namespace Kakoune
{

Editor::Editor(Buffer& buffer)
    : m_buffer(buffer),
      m_edition_level(0)
{
    m_selections.push_back(Selection(buffer.begin(), buffer.begin()));
}

void Editor::erase()
{
    scoped_edition edition(*this);
    for (auto& sel : m_selections)
    {
        m_buffer.erase(sel.begin(), sel.end());
        sel.selection.avoid_eol();
    }
}

static BufferIterator prepare_insert(Buffer& buffer, const Selection& sel,
                                     InsertMode mode)
{
    switch (mode)
    {
    case InsertMode::Insert:
    case InsertMode::Replace:
        return sel.begin();
    case InsertMode::Append:
        return sel.end();
    case InsertMode::InsertAtLineBegin:
        return buffer.iterator_at_line_begin(sel.begin());
    case InsertMode::AppendAtLineEnd:
        return buffer.iterator_at_line_end(sel.end()-1);
    case InsertMode::OpenLineBelow:
    {
        auto pos = buffer.iterator_at_line_end(sel.end() - 1);
        buffer.insert(pos, "\n");
        return buffer.iterator_at_line_begin(pos.line() + 1);
    }
    case InsertMode::OpenLineAbove:
    {
        auto pos = buffer.iterator_at_line_begin(sel.begin());
        buffer.insert(pos, "\n");
        return pos;
    }
    }
    assert(false);
    return BufferIterator{};
}

void Editor::insert(const String& string, InsertMode mode)
{
    scoped_edition edition(*this);
    if (mode == InsertMode::Replace)
        erase();

    for (auto& sel : m_selections)
    {
        BufferIterator pos = prepare_insert(m_buffer, sel.selection, mode);
        m_buffer.insert(pos, string);
    }
}

void Editor::insert(const memoryview<String>& strings, InsertMode mode)
{
    scoped_edition edition(*this);
    if (mode == InsertMode::Replace)
        erase();

    if (strings.empty())
        return;

    for (size_t i = 0; i < selections().size(); ++i)
    {
        BufferIterator pos = prepare_insert(m_buffer, m_selections[i].selection, mode);
        size_t index = std::min(i, strings.size()-1);
        m_buffer.insert(pos, strings[index]);
    }
}

std::vector<String> Editor::selections_content() const
{
    std::vector<String> contents;
    for (auto& sel : m_selections)
        contents.push_back(m_buffer.string(sel.begin(),
                                           sel.end()));
    return contents;
}

static bool overlaps(const SelectionAndCaptures& lhs,
                     const SelectionAndCaptures& rhs)
{
    return (lhs.first() <= rhs.first() and lhs.last() >= rhs.first()) or
           (lhs.first() <= rhs.last()  and lhs.last() >= rhs.last());
}

static void merge_overlapping(SelectionAndCapturesList& selections)
{
    for (size_t i = 0; i < selections.size(); ++i)
    {
        for (size_t j = i+1; j < selections.size();)
        {
            if (overlaps(selections[i], selections[j]))
            {
                selections[i].selection.merge_with(selections[j].selection);
                selections.erase(selections.begin() + j);
            }
            else
               ++j;
        }
    }
}

void Editor::move_selections(CharCount offset, SelectMode mode)
{
    assert(mode == SelectMode::Replace or mode == SelectMode::Extend);
    for (auto& sel : m_selections)
    {
        auto last = sel.last();
        auto limit = offset < 0 ? buffer().iterator_at_line_begin(last)
                                : utf8::previous(buffer().iterator_at_line_end(last));
        last = utf8::advance(last, limit, offset);
        sel.selection = Selection(mode == SelectMode::Extend ? sel.first() : last, last);
    }
    merge_overlapping(m_selections);
}

void Editor::move_selections(LineCount offset, SelectMode mode)
{
    assert(mode == SelectMode::Replace or mode == SelectMode::Extend);
    for (auto& sel : m_selections)
    {
        BufferCoord pos = m_buffer.line_and_column_at(sel.last());
        pos.line += offset;
        BufferIterator last = utf8::finish(m_buffer.iterator_at(pos, true));
        sel.selection = Selection(mode == SelectMode::Extend ? sel.first() : last, last);
    }
    merge_overlapping(m_selections);
}

void Editor::clear_selections()
{
    check_invariant();
    BufferIterator pos = m_selections.back().last();

    if (*pos == '\n' and not pos.is_begin() and *utf8::previous(pos) != '\n')
        pos = utf8::previous(pos);

    Selection sel = Selection(pos, pos);
    m_selections.clear();
    m_selections.push_back(std::move(sel));
}

void Editor::keep_selection(int index)
{
    check_invariant();

    if (index < m_selections.size())
    {
        SelectionAndCaptures sel = std::move(m_selections[index]);
        m_selections.clear();
        m_selections.push_back(std::move(sel));
    }
}

void Editor::remove_selection(int index)
{
    check_invariant();

    if (m_selections.size() > 1 and index < m_selections.size())
        m_selections.erase(m_selections.begin() + index);
}

void Editor::select(const BufferIterator& iterator)
{
    m_selections.clear();
    m_selections.push_back(Selection(iterator, iterator));
}

void Editor::select(SelectionAndCapturesList selections)
{
    if (selections.empty())
        throw runtime_error("no selections");
    m_selections = std::move(selections);
}

void Editor::select(const Selector& selector, SelectMode mode)
{
    check_invariant();

    if (mode == SelectMode::Append)
    {
        auto& sel = m_selections.back();
        SelectionAndCaptures res = selector(sel.selection);
        if (res.captures.empty())
            res.captures = sel.captures;
        m_selections.push_back(res);
    }
    else
    {
        for (auto& sel : m_selections)
        {
            SelectionAndCaptures res = selector(sel.selection);
            if (mode == SelectMode::Extend)
                sel.selection.merge_with(res.selection);
            else
                sel.selection = std::move(res.selection);

            if (not res.captures.empty())
                sel.captures = std::move(res.captures);
        }
    }
    merge_overlapping(m_selections);
}

struct nothing_selected : public runtime_error
{
    nothing_selected() : runtime_error("nothing was selected") {}
};

void Editor::multi_select(const MultiSelector& selector)
{
    check_invariant();

    SelectionAndCapturesList new_selections;
    for (auto& sel : m_selections)
    {
        SelectionAndCapturesList res = selector(sel.selection);
        for (auto& sel_and_cap : res)
        {
            // preserve captures when selectors captures nothing.
            if (sel_and_cap.captures.empty())
                new_selections.emplace_back(sel_and_cap.selection, sel.captures);
            else
                new_selections.push_back(std::move(sel_and_cap));
        }
    }
    if (new_selections.empty())
        throw nothing_selected();

    merge_overlapping(new_selections);
    m_selections = std::move(new_selections);
}

class LastModifiedRangeListener : public BufferChangeListener
{
public:
    LastModifiedRangeListener(Buffer& buffer)
       : m_buffer(buffer)
    { m_buffer.add_change_listener(*this); }

    ~LastModifiedRangeListener()
    { m_buffer.remove_change_listener(*this); }

    void on_insert(const BufferIterator& begin, const BufferIterator& end)
    {
        m_first = begin;
        m_last = utf8::previous(end);
    }

    void on_erase(const BufferIterator& begin, const BufferIterator& end)
    {
        m_first = begin;
        if (m_first >= m_buffer.end())
            m_first = utf8::previous(m_buffer.end());
        m_last = m_first;
    }

    const BufferIterator& first() const { return m_first; }
    const BufferIterator& last() const { return m_last; }

private:
    BufferIterator m_first;
    BufferIterator m_last;
    Buffer& m_buffer;
};

bool Editor::undo()
{
    LastModifiedRangeListener listener(buffer());
    bool res = m_buffer.undo();
    if (res)
    {
        m_selections.clear();
        m_selections.push_back(Selection(listener.first(),
                                         listener.last()));
    }
    return res;
}

bool Editor::redo()
{
    LastModifiedRangeListener listener(buffer());
    bool res = m_buffer.redo();
    if (res)
    {
        m_selections.clear();
        m_selections.push_back(Selection(listener.first(),
                                         listener.last()));
    }
    return res;
}

void Editor::check_invariant() const
{
    assert(not m_selections.empty());
}

void Editor::begin_edition()
{
    ++m_edition_level;

    if (m_edition_level == 1)
        m_buffer.begin_undo_group();
}

void Editor::end_edition()
{
    assert(m_edition_level > 0);
    if (m_edition_level == 1)
        m_buffer.end_undo_group();

    --m_edition_level;
}

using utf8_it = utf8::utf8_iterator<BufferIterator>;

IncrementalInserter::IncrementalInserter(Editor& editor, InsertMode mode)
    : m_editor(editor), m_edition(editor), m_mode(mode)
{
    m_editor.on_incremental_insertion_begin();
    Buffer& buffer = editor.m_buffer;

    if (mode == InsertMode::Replace)
    {
        for (auto& sel : editor.m_selections)
            buffer.erase(sel.begin(), sel.end());
    }

    for (auto& sel : m_editor.m_selections)
    {
        utf8_it first, last;
        switch (mode)
        {
        case InsertMode::Insert:  first = utf8_it(sel.end()) - 1; last = sel.begin(); break;
        case InsertMode::Replace: first = utf8_it(sel.end()) - 1; last = sel.begin(); break;
        case InsertMode::Append:  first = sel.begin(); last = sel.end(); break;

        case InsertMode::OpenLineBelow:
        case InsertMode::AppendAtLineEnd:
            first = utf8_it(buffer.iterator_at_line_end(utf8::previous(sel.end()))) - 1;
            last  = first;
            break;

        case InsertMode::OpenLineAbove:
        case InsertMode::InsertAtLineBegin:
            first = buffer.iterator_at_line_begin(sel.begin());
            if (mode == InsertMode::OpenLineAbove)
                --first;
            else
            {
                auto first_non_blank = first;
                while (*first_non_blank == ' ' or *first_non_blank == '\t')
                    ++first_non_blank;
                if (*first_non_blank != '\n')
                    first = first_non_blank;
            }
            last = first;
            break;
        }
        if (first.underlying_iterator().is_end())
           --first;
        if (last.underlying_iterator().is_end())
           --last;
        sel.selection = Selection(first.underlying_iterator(), last.underlying_iterator());
    }
    if (mode == InsertMode::OpenLineBelow or mode == InsertMode::OpenLineAbove)
    {
        insert("\n");
        if (mode == InsertMode::OpenLineAbove)
        {
            for (auto& sel : m_editor.m_selections)
            {
                // special case, the --first line above did nothing, so we need to compensate now
                if (sel.first() == utf8::next(buffer.begin()))
                    sel.selection = Selection(buffer.begin(), buffer.begin());
            }
        }
    }
}

IncrementalInserter::~IncrementalInserter()
{
    for (auto& sel : m_editor.m_selections)
    {
        if (m_mode == InsertMode::Append)
            sel = Selection(sel.first(), utf8::previous(sel.last()));
         sel.selection.avoid_eol();
    }

    m_editor.on_incremental_insertion_end();
}

void IncrementalInserter::insert(const String& string)
{
    Buffer& buffer = m_editor.buffer();
    for (auto& sel : m_editor.m_selections)
    {
        BufferIterator position = sel.last();
        String content = string;
        m_editor.filters()(buffer, position, content);
        m_editor.buffer().insert(position, content);
    }
}

void IncrementalInserter::insert(const memoryview<String>& strings)
{
    m_editor.insert(strings);
}

void IncrementalInserter::erase()
{
    for (auto& sel : m_editor.m_selections)
    {
        BufferIterator pos = sel.last();
        m_editor.buffer().erase(utf8::previous(pos), pos);
    }
}

void IncrementalInserter::move_cursors(const BufferCoord& offset)
{
    for (auto& sel : m_editor.m_selections)
    {
        BufferCoord pos = m_editor.m_buffer.line_and_column_at(sel.last());
        BufferIterator it = m_editor.m_buffer.iterator_at(pos + offset);
        sel = Selection(it, it);
    }
}

}
