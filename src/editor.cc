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
    m_selections.push_back(SelectionList());
    m_selections.back().push_back(Selection(buffer.begin(), buffer.begin()));
}

void Editor::erase()
{
    scoped_edition edition(*this);
    for (auto& sel : selections())
        m_buffer.modify(Modification::make_erase(sel.begin(), sel.end()));
}

template<bool append>
static void do_insert(Editor& editor, const String& string)
{
    scoped_edition edition(editor);
    for (auto& sel : editor.selections())
    {
        BufferIterator pos = append ? sel.end() : sel.begin();
        editor.buffer().modify(Modification::make_insert(pos, string));
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
        editor.buffer().modify(Modification::make_insert(pos, strings[index]));
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
    for (auto& sel : selections())
        contents.push_back(m_buffer.string(sel.begin(), sel.end()));
    return contents;
}

void Editor::push_selections()
{
    SelectionList current_selections = m_selections.back();
    m_selections.push_back(std::move(current_selections));
}

void Editor::pop_selections()
{
    if (m_selections.size() > 1)
        m_selections.pop_back();
    else
        throw runtime_error("no more selections on stack");
}

void Editor::move_selections(const BufferCoord& offset, bool append)
{
    for (auto& sel : m_selections.back())
    {
        BufferCoord pos = m_buffer.line_and_column_at(sel.last());
        BufferIterator last = m_buffer.iterator_at(pos + BufferCoord(offset));
        sel = Selection(append ? sel.first() : last, last);
    }
}

void Editor::clear_selections()
{
    check_invariant();
    BufferIterator pos = selections().back().last();

    if (*pos == '\n' and not pos.is_begin() and *(pos-1) != '\n')
        --pos;

    Selection sel = Selection(pos, pos);
    m_selections.back().clear();
    m_selections.back().push_back(std::move(sel));
}

void Editor::keep_selection(int index)
{
    check_invariant();

    if (index < selections().size())
    {
        Selection sel = selections()[index];
        m_selections.back().clear();
        m_selections.back().push_back(std::move(sel));
    }
}

void Editor::remove_selection(int index)
{
    check_invariant();

    if (selections().size() > 1 and index < selections().size())
        m_selections.back().erase(m_selections.back().begin() + index);
}

void Editor::select(const BufferIterator& iterator)
{
    m_selections.back().clear();
    m_selections.back().push_back(Selection(iterator, iterator));
}

void Editor::select(const Selector& selector, bool append)
{
    check_invariant();

    std::array<std::vector<String>, 10> captures;

    size_t capture_count = -1;
    for (auto& sel : m_selections.back())
    {
        SelectionAndCaptures res = selector(sel);
        if (append)
            sel.merge_with(res.selection);
        else
            sel = std::move(res.selection);

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
    SelectionList new_selections;
    for (auto& sel : m_selections.back())
    {
        SelectionAndCapturesList res = selector(sel);
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

    m_selections.back() = std::move(new_selections);

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
        m_selections.back().clear();
        m_selections.back().push_back(Selection(listener.first(),
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
        m_selections.back().clear();
        m_selections.back().push_back(Selection(listener.first(),
                                                listener.last()));
    }
    return res;
}

void Editor::check_invariant() const
{
    assert(not selections().empty());
}

struct id_not_unique : public runtime_error
{
    id_not_unique(const String& id)
        : runtime_error("id not unique: " + id) {}
};

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

IncrementalInserter::IncrementalInserter(Editor& editor, Mode mode)
    : m_editor(editor), m_edition(editor), m_mode(mode)
{
    m_editor.on_incremental_insertion_begin();

    if (mode == Mode::Change)
        editor.erase();

    for (auto& sel : m_editor.m_selections.back())
    {
        BufferIterator first, last;
        switch (mode)
        {
        case Mode::Insert: first = sel.end()-1; last = sel.begin(); break;
        case Mode::Change: first = sel.end()-1; last = sel.begin(); break;
        case Mode::Append: first = sel.begin(); last = sel.end(); break;

        case Mode::OpenLineBelow:
        case Mode::AppendAtLineEnd:
            first = m_editor.m_buffer.iterator_at_line_end(sel.end() - 1) - 1;
            last  = first;
            break;

        case Mode::OpenLineAbove:
        case Mode::InsertAtLineBegin:
            first = m_editor.m_buffer.iterator_at_line_begin(sel.begin());
            if (mode == Mode::OpenLineAbove)
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
        sel = Selection(first, last);

        if (mode == Mode::OpenLineBelow or mode == Mode::OpenLineAbove)
            apply(Modification::make_insert(sel.last(), "\n"));
    }
}

IncrementalInserter::~IncrementalInserter()
{
    if (m_mode == Mode::Append)
        for (auto& sel : m_editor.m_selections.back())
            sel = Selection(sel.first(), sel.last()-1);

    m_editor.on_incremental_insertion_end();
}

void IncrementalInserter::apply(Modification&& modification) const
{
    m_editor.filters()(m_editor.buffer(), modification);
    m_editor.buffer().modify(std::move(modification));
}

void IncrementalInserter::insert(const String& string)
{
    for (auto& sel : m_editor.selections())
        apply(Modification::make_insert(sel.last(), string));
}

void IncrementalInserter::insert(const memoryview<String>& strings)
{
    m_editor.insert(strings);
}

void IncrementalInserter::erase()
{
    for (auto& sel : m_editor.m_selections.back())
    {
        BufferIterator pos = sel.last();
        apply(Modification::make_erase(pos-1, pos));
    }
}

void IncrementalInserter::move_cursors(const BufferCoord& offset)
{
    for (auto& sel : m_editor.m_selections.back())
    {
        BufferCoord pos = m_editor.m_buffer.line_and_column_at(sel.last());
        BufferIterator it = m_editor.m_buffer.iterator_at(pos + offset);
        sel = Selection(it, it);
    }
}

}
