#include "editor.hh"

#include "exception.hh"
#include "utils.hh"
#include "register.hh"
#include "register_manager.hh"

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

template<bool append>
static void do_insert(Editor& editor, const String& string)
{
    scoped_edition edition(editor);
    for (auto& sel : editor.selections())
    {
        BufferIterator pos = append ? sel.end()
                                    : sel.begin();
        editor.buffer().insert(pos, string);
    }
}

template<bool append>
static void do_insert(Editor& editor, const memoryview<String>& strings)
{
    if (strings.empty())
        return;

    scoped_edition edition(editor);
    for (size_t i = 0; i < editor.selections().size(); ++i)
    {
        BufferIterator pos = append ? editor.selections()[i].end()
                                    : editor.selections()[i].begin();
        size_t index = std::min(i, strings.size()-1);
        editor.buffer().insert(pos, strings[index]);
    }
}

void Editor::insert(const String& string)
{
    do_insert<false>(*this, string);
}

void Editor::insert(const memoryview<String>& strings)
{
    do_insert<false>(*this, strings);
}

void Editor::append(const String& string)
{
    do_insert<true>(*this, string);
}

void Editor::append(const memoryview<String>& strings)
{
    do_insert<true>(*this, strings);
}

void Editor::replace(const String& string)
{
    scoped_edition edition(*this);
    erase();
    insert(string);
}

void Editor::replace(const memoryview<String>& strings)
{
    scoped_edition edition(*this);
    erase();
    insert(strings);
}

std::vector<String> Editor::selections_content() const
{
    std::vector<String> contents;
    for (auto& sel : m_selections)
        contents.push_back(m_buffer.string(sel.begin(),
                                           sel.end()));
    return contents;
}

void Editor::move_selections(const BufferCoord& offset, SelectMode mode)
{
    assert(mode == SelectMode::Replace or mode == SelectMode::Extend);
    for (auto& sel : m_selections)
    {
        BufferCoord pos = m_buffer.line_and_column_at(sel.last());
        BufferIterator last = m_buffer.iterator_at(pos + offset, true);
        sel.selection = Selection(mode == SelectMode::Extend ? sel.first() : last, last);
    }
}

void Editor::clear_selections()
{
    check_invariant();
    BufferIterator pos = m_selections.back().last();

    if (*pos == '\n' and not pos.is_begin() and *(pos-1) != '\n')
        --pos;

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

    std::array<std::vector<String>, 10> captures;

    size_t capture_count = -1;
    for (auto& sel : m_selections)
    {
        SelectionAndCaptures res = selector(sel.selection);
        if (mode == SelectMode::Extend)
            sel.selection.merge_with(res.selection);
        else
            sel.selection = std::move(res.selection);

        assert(capture_count == -1 or capture_count == res.captures.size());
        capture_count = res.captures.size();

        for (size_t i = 0; i < res.captures.size(); ++i)
            captures[i].push_back(res.captures[i]);
    }

    if (not captures[0].empty())
    {
        for (size_t i = 0; i < captures.size(); ++i)
            RegisterManager::instance()['0' + i] = std::move(captures[i]);
    }
}

struct nothing_selected : public runtime_error
{
    nothing_selected() : runtime_error("nothing was selected") {}
};

void Editor::multi_select(const MultiSelector& selector)
{
    check_invariant();

    std::array<std::vector<String>, 10> captures;

    size_t capture_count = -1;
    SelectionAndCapturesList new_selections;
    for (auto& sel : m_selections)
    {
        SelectionAndCapturesList res = selector(sel.selection);
        for (auto& sel_and_cap : res)
        {
            new_selections.push_back(sel_and_cap.selection);

            assert(capture_count == -1 or
                   capture_count == sel_and_cap.captures.size());
            capture_count = sel_and_cap.captures.size();

            for (size_t i = 0; i < sel_and_cap.captures.size(); ++i)
                captures[i].push_back(sel_and_cap.captures[i]);
        }
    }
    if (new_selections.empty())
        throw nothing_selected();

    m_selections = std::move(new_selections);

    if (not captures[0].empty())
    {
        for (size_t i = 0; i < captures.size(); ++i)
            RegisterManager::instance()['0' + i] = std::move(captures[i]);
    }
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
        m_last = end-1;
    }

    void on_erase(const BufferIterator& begin, const BufferIterator& end)
    {
        m_first = begin;
        if (m_first >= m_buffer.end())
            m_first = m_buffer.end() - 1;
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

IncrementalInserter::IncrementalInserter(Editor& editor, InsertMode mode)
    : m_editor(editor), m_edition(editor), m_mode(mode)
{
    m_editor.on_incremental_insertion_begin();

    if (mode == InsertMode::Change)
    {
        for (auto& sel : editor.m_selections)
            editor.m_buffer.erase(sel.begin(), sel.end());
    }

    for (auto& sel : m_editor.m_selections)
    {
        BufferIterator first, last;
        switch (mode)
        {
        case InsertMode::Insert: first = sel.end()-1; last = sel.begin(); break;
        case InsertMode::Change: first = sel.end()-1; last = sel.begin(); break;
        case InsertMode::Append: first = sel.begin(); last = sel.end(); break;

        case InsertMode::OpenLineBelow:
        case InsertMode::AppendAtLineEnd:
            first = m_editor.m_buffer.iterator_at_line_end(sel.end() - 1) - 1;
            last  = first;
            break;

        case InsertMode::OpenLineAbove:
        case InsertMode::InsertAtLineBegin:
            first = m_editor.m_buffer.iterator_at_line_begin(sel.begin());
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
        if (first.is_end())
           --first;
        if (last.is_end())
           --last;
        sel = Selection(first, last);
    }
    if (mode == InsertMode::OpenLineBelow or mode == InsertMode::OpenLineAbove)
    {
        insert("\n");
        if (mode == InsertMode::OpenLineAbove)
        {
            for (auto& sel : m_editor.m_selections)
            {
                // special case, the --first line above did nothing, so we need to compensate now
                if (sel.first() == buffer().begin() + 1)
                    sel = Selection(buffer().begin(), buffer().begin());
            }
        }
    }
}

IncrementalInserter::~IncrementalInserter()
{
    for (auto& sel : m_editor.m_selections)
    {
        if (m_mode == InsertMode::Append)
            sel = Selection(sel.first(), sel.last()-1);
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
        m_editor.buffer().erase(pos-1, pos);
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
