#ifndef editor_hh_INCLUDED
#define editor_hh_INCLUDED

#include "buffer.hh"
#include "dynamic_selection_list.hh"
#include "filter.hh"
#include "memoryview.hh"

namespace Kakoune
{

class Register;

enum class SelectMode
{
    Replace,
    Extend,
    Append,
    ReplaceMain,
};

enum class InsertMode : unsigned
{
    Insert,
    Append,
    Replace,
    InsertAtLineBegin,
    InsertAtNextLineBegin,
    AppendAtLineEnd,
    OpenLineBelow,
    OpenLineAbove
};

// An Editor is a buffer mutator
//
// The Editor class provides methods to manipulate a set of selections
// and to use these selections to mutate it's buffer.
class Editor : public SafeCountable
{
public:
    typedef std::function<Selection (const Buffer&, const Selection&)> Selector;
    typedef std::function<SelectionList (const Buffer&, const Selection&)>  MultiSelector;

    Editor(Buffer& buffer);
    virtual ~Editor() {}

    Buffer& buffer() const { return *m_buffer; }

    void erase();

    void insert(const String& string,
                InsertMode mode = InsertMode::Insert);
    void insert(const memoryview<String>& strings,
                InsertMode mode = InsertMode::Insert);

    void move_selections(LineCount move,
                         SelectMode mode = SelectMode::Replace);
    void move_selections(CharCount move,
                         SelectMode mode = SelectMode::Replace);
    void clear_selections();
    void flip_selections();
    void keep_selection(int index);
    void remove_selection(int index);
    void select(const BufferCoord& c, SelectMode mode = SelectMode::Replace)
    { select(Selection{ buffer().clamp(c) }, mode); }
    void select(const Selection& sel,
                SelectMode mode = SelectMode::Replace);
    void select(const Selector& selector,
                SelectMode mode = SelectMode::Replace);
    void select(SelectionList selections);
    void multi_select(const MultiSelector& selector);

    void rotate_selections(int count) { m_main_sel = (m_main_sel + count) % m_selections.size(); }

    const SelectionList& selections() const { return m_selections; }
    const Selection& main_selection() const { return m_selections[m_main_sel]; }
    size_t main_selection_index() const { return m_main_sel; }
    std::vector<String>  selections_content() const;

    bool undo();
    bool redo();

    FilterGroup& filters() { return m_filters; }

    CandidateList complete_filterid(const String& prefix,
                                    size_t cursor_pos = String::npos);

    bool is_editing() const { return m_edition_level!= 0; }
private:
    friend struct scoped_edition;
    void begin_edition();
    void end_edition();

    virtual BufferCoord offset_coord(const BufferCoord& coord, LineCount move);
    virtual BufferCoord offset_coord(const BufferCoord& coord, CharCount move);

    int m_edition_level;

    void check_invariant() const;

    friend class IncrementalInserter;

    safe_ptr<Buffer>         m_buffer;
    DynamicSelectionList     m_selections;
    size_t                   m_main_sel;
    FilterGroup              m_filters;
};

struct scoped_edition
{
    scoped_edition(Editor& editor)
        : m_editor(editor)
    { m_editor.begin_edition(); }

    ~scoped_edition()
    { m_editor.end_edition(); }
private:
    Editor& m_editor;
};

// An IncrementalInserter manage insert mode
class IncrementalInserter
{
public:
    IncrementalInserter(Editor& editor, InsertMode mode = InsertMode::Insert);
    ~IncrementalInserter();

    void insert(String content);
    void insert(const memoryview<String>& strings);
    void erase();
    void move_cursors(CharCount move);
    void move_cursors(LineCount move);

    Buffer& buffer() const { return m_editor.buffer(); }
    Editor& editor() const { return m_editor; }

private:
    InsertMode     m_mode;
    Editor&        m_editor;
    scoped_edition m_edition;
};

}

#endif // editor_hh_INCLUDED

