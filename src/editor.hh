#ifndef editor_hh_INCLUDED
#define editor_hh_INCLUDED

#include "buffer.hh"
#include "dynamic_selection_list.hh"
#include "memoryview.hh"

namespace Kakoune
{

namespace InputModes { class Insert; }

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
    typedef std::function<SelectionList (const Buffer&, SelectionList)>  MultiSelector;

    Editor(Buffer& buffer);
    virtual ~Editor() {}

    Buffer& buffer() const { return *m_buffer; }

    void erase();

    void insert(const String& string,
                InsertMode mode = InsertMode::Insert);
    void insert(memoryview<String> strings,
                InsertMode mode = InsertMode::Insert);

    void move_selections(LineCount move,
                         SelectMode mode = SelectMode::Replace);
    void move_selections(CharCount move,
                         SelectMode mode = SelectMode::Replace);
    void clear_selections();
    void flip_selections();
    void keep_selection(int index);
    void remove_selection(int index);
    void select(BufferCoord c, SelectMode mode = SelectMode::Replace)
    { select(Selection{ buffer().clamp(c) }, mode); }
    void select(const Selection& sel,
                SelectMode mode = SelectMode::Replace);
    void select(const Selector& selector,
                SelectMode mode = SelectMode::Replace);
    void select(SelectionList selections);
    void multi_select(const MultiSelector& selector);

    void rotate_selections(int count) { m_selections.rotate_main(count); }

    const SelectionList& selections() const { return m_selections; }
    const Selection& main_selection() const { return m_selections.main(); }
    size_t main_selection_index() const { return m_selections.main_index(); }
    std::vector<String>  selections_content() const;

    bool undo();
    bool redo();

    bool is_editing() const { return m_edition_level!= 0; }
private:
    friend struct scoped_edition;
    friend class InputModes::Insert;
    void begin_edition();
    void end_edition();

    virtual BufferCoord offset_coord(BufferCoord coord, LineCount move);
    virtual BufferCoord offset_coord(BufferCoord coord, CharCount move);

    int m_edition_level;

    void check_invariant() const;

    safe_ptr<Buffer>         m_buffer;
    DynamicSelectionList     m_selections;
};

struct scoped_edition
{
    scoped_edition(Editor& editor)
        : m_editor(editor)
    { m_editor.begin_edition(); }

    ~scoped_edition()
    { m_editor.end_edition(); }

    Editor& editor() const { return m_editor; }
private:
    Editor& m_editor;
};

void avoid_eol(const Buffer& buffer, BufferCoord& coord);
void avoid_eol(const Buffer& buffer, Range& sel);

}

#endif // editor_hh_INCLUDED

