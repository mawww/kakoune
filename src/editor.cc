#include "editor.hh"

#include "exception.hh"
#include "utils.hh"

namespace Kakoune
{

Editor::Editor(Buffer& buffer)
    : m_buffer(buffer),
      m_edition_level(0)
{
    m_selections.push_back(SelectionList());
    selections().push_back(Selection(buffer.begin(), buffer.begin()));
}

void Editor::erase()
{
    scoped_edition edition(*this);
    for (auto& sel : selections())
        m_buffer.modify(Modification::make_erase(sel.begin(), sel.end()));
}

void Editor::insert(const String& string)
{
    scoped_edition edition(*this);
    for (auto& sel : selections())
        m_buffer.modify(Modification::make_insert(sel.begin(), string));
}

void Editor::append(const String& string)
{
    scoped_edition edition(*this);
    for (auto& sel : selections())
        m_buffer.modify(Modification::make_insert(sel.end(), string));
}

void Editor::replace(const std::string& string)
{
    scoped_edition edition(*this);
    erase();
    insert(string);
}

void Editor::push_selections()
{
    SelectionList current_selections = selections();
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
    for (auto& sel : selections())
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
    selections().clear();
    selections().push_back(std::move(sel));
}

void Editor::keep_selection(int index)
{
    check_invariant();

    if (index < selections().size())
    {
        Selection sel = selections()[index];
        selections().clear();
        selections().push_back(std::move(sel));
    }
}

void Editor::select(const BufferIterator& iterator)
{
    selections().clear();
    selections().push_back(Selection(iterator, iterator));
}

void Editor::select(const Selector& selector, bool append)
{
    check_invariant();

    if (not append)
    {
        for (auto& sel : selections())
            sel = selector(sel);
    }
    else
    {
        for (auto& sel : selections())
            sel.merge_with(selector(sel));
    }
}

struct nothing_selected : public runtime_error
{
    nothing_selected() : runtime_error("nothing was selected") {}
};

void Editor::multi_select(const MultiSelector& selector)
{
    check_invariant();

    SelectionList new_selections;
    for (auto& sel : selections())
    {
        SelectionList selections = selector(sel);
        std::copy(selections.begin(), selections.end(),
                  std::back_inserter(new_selections));
    }
    if (new_selections.empty())
        throw nothing_selected();

    selections() = std::move(new_selections);
}

BufferString Editor::selection_content() const
{
    check_invariant();

    return m_buffer.string(selections().back().begin(),
                           selections().back().end());
}

bool Editor::undo()
{
    return m_buffer.undo();
}

bool Editor::redo()
{
    return m_buffer.redo();
}

void Editor::check_invariant() const
{
    assert(not selections().empty());
}

struct id_not_unique : public runtime_error
{
    id_not_unique(const std::string& id)
        : runtime_error("id not unique: " + id) {}
};

void Editor::add_filter(FilterAndId&& filter)
{
    if (m_filters.contains(filter.first))
        throw id_not_unique(filter.first);
    m_filters.append(std::forward<FilterAndId>(filter));
}

void Editor::remove_filter(const std::string& id)
{
    m_filters.remove(id);
}

CandidateList Editor::complete_filterid(const std::string& prefix,
                                        size_t cursor_pos)
{
    return m_filters.complete_id<str_to_str>(prefix, cursor_pos);
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

IncrementalInserter::IncrementalInserter(Editor& editor, Mode mode)
    : m_editor(editor), m_edition(editor)
{
    m_editor.on_incremental_insertion_begin();

    if (mode == Mode::Change)
        editor.erase();

    for (auto& sel : m_editor.selections())
    {
        BufferIterator pos;
        switch (mode)
        {
        case Mode::Insert: pos = sel.begin(); break;
        case Mode::Append: pos = sel.end(); break;
        case Mode::Change: pos = sel.begin(); break;

        case Mode::OpenLineBelow:
        case Mode::AppendAtLineEnd:
            pos = m_editor.m_buffer.iterator_at_line_end(sel.end() - 1) - 1;
            break;

        case Mode::OpenLineAbove:
        case Mode::InsertAtLineBegin:
            pos = m_editor.m_buffer.iterator_at_line_begin(sel.begin());
            if (mode == Mode::OpenLineAbove)
                --pos;
            break;
        }
        sel = Selection(pos, pos, sel.captures());

        if (mode == Mode::OpenLineBelow or mode == Mode::OpenLineAbove)
            apply(Modification::make_insert(pos, "\n"));
    }
}

IncrementalInserter::~IncrementalInserter()
{
    move_cursors(BufferCoord(0, -1));
    m_editor.on_incremental_insertion_end();
}

void IncrementalInserter::apply(Modification&& modification) const
{
    for (auto filter : m_editor.m_filters)
        filter.second(m_editor.buffer(), modification);
    m_editor.buffer().modify(std::move(modification));
}


void IncrementalInserter::insert(const Editor::String& string)
{
    for (auto& sel : m_editor.selections())
        apply(Modification::make_insert(sel.begin(), string));
}

void IncrementalInserter::insert_capture(size_t index)
{
    for (auto& sel : m_editor.selections())
        m_editor.m_buffer.modify(Modification::make_insert(sel.begin(),
                                                           sel.capture(index)));
}

void IncrementalInserter::erase()
{
    for (auto& sel : m_editor.selections())
    {
        sel = Selection(sel.first() - 1, sel.last() - 1);
        apply(Modification::make_erase(sel.begin(), sel.end()));
    }
}

void IncrementalInserter::move_cursors(const BufferCoord& offset)
{
    for (auto& sel : m_editor.selections())
    {
        BufferCoord pos = m_editor.m_buffer.line_and_column_at(sel.last());
        BufferIterator it = m_editor.m_buffer.iterator_at(pos + offset);
        sel = Selection(it, it);
    }
}

}
