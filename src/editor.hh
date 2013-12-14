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
    typedef std::function<void (const Buffer&, SelectionList&)>  Selector;

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

    const SelectionList& selections() const { return m_selections; }
    SelectionList& selections() { return m_selections; }
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

}

#endif // editor_hh_INCLUDED

