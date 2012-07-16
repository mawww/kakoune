#ifndef editor_hh_INCLUDED
#define editor_hh_INCLUDED

#include "buffer.hh"
#include "selection.hh"
#include "filter.hh"
#include "idvaluemap.hh"
#include "memoryview.hh"
#include "filter_group.hh"

namespace Kakoune
{

class Register;

// An Editor is a buffer mutator
//
// The Editor class provides methods to manipulate a set of selections
// and to use these selections to mutate it's buffer.
class Editor
{
public:
    typedef std::function<SelectionAndCaptures (const Selection&)> Selector;
    typedef std::function<SelectionAndCapturesList (const Selection&)>  MultiSelector;

    Editor(Buffer& buffer);
    virtual ~Editor() {}

    Buffer& buffer() const { return m_buffer; }

    void erase();

    void insert(const String& string);
    void insert(const memoryview<String>& strings);

    void append(const String& string);
    void append(const memoryview<String>& strings);

    void replace(const String& string);
    void replace(const memoryview<String>& strings);

    void push_selections();
    void pop_selections();

    void move_selections(const BufferCoord& offset, bool append = false);
    void clear_selections();
    void keep_selection(int index);
    void remove_selection(int index);
    void select(const BufferIterator& iterator);
    void select(const Selector& selector, bool append = false);
    void multi_select(const MultiSelector& selector);

    const SelectionList& selections() const { return m_selections.back(); }
    std::vector<String>  selections_content() const;

    bool undo();
    bool redo();

    FilterGroup& filters() { return m_filters; }

    CandidateList complete_filterid(const String& prefix,
                                    size_t cursor_pos = String::npos);

    bool is_editing() const { return m_edition_level!= 0; }

private:
    friend class scoped_edition;
    void begin_edition();
    void end_edition();

    int m_edition_level;

    void check_invariant() const;

    friend class IncrementalInserter;
    virtual void on_incremental_insertion_begin() {}
    virtual void on_incremental_insertion_end() {}

    Buffer&                             m_buffer;
    std::vector<SelectionList>          m_selections;
    FilterGroup                         m_filters;
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
    enum class Mode
    {
        Insert,
        Append,
        Change,
        InsertAtLineBegin,
        AppendAtLineEnd,
        OpenLineBelow,
        OpenLineAbove
    };

    IncrementalInserter(Editor& editor, Mode mode = Mode::Insert);
    ~IncrementalInserter();

    void insert(const String& string);
    void insert(const memoryview<String>& strings);
    void erase();
    void move_cursors(const BufferCoord& offset);

    Buffer& buffer() const { return m_editor.buffer(); }

private:
    void apply(Modification&& modification) const;

    Mode    m_mode;
    Editor& m_editor;
    scoped_edition m_edition;
};

}

#endif // editor_hh_INCLUDED

