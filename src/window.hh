#ifndef window_hh_INCLUDED
#define window_hh_INCLUDED

#include <functional>

#include "buffer.hh"
#include "dynamic_buffer_iterator.hh"
#include "display_buffer.hh"
#include "completion.hh"
#include "highlighter.hh"
#include "highlighter_group.hh"
#include "filter.hh"
#include "idvaluemap.hh"
#include "hooks_manager.hh"

namespace Kakoune
{

struct Selection
{
    typedef std::vector<BufferString> CaptureList;

    Selection(const BufferIterator& first, const BufferIterator& last,
              const CaptureList& captures = CaptureList())
        : m_first(first), m_last(last), m_captures(captures) {}

    Selection(const BufferIterator& first, const BufferIterator& last,
              CaptureList&& captures)
        : m_first(first), m_last(last), m_captures(captures) {}

    BufferIterator begin() const;
    BufferIterator end() const;

    const BufferIterator& first() const { return m_first; }
    const BufferIterator& last()  const { return m_last; }

    void merge_with(const Selection& selection);

    BufferString capture(size_t index) const;
    const CaptureList& captures() const { return m_captures; }

private:
    DynamicBufferIterator m_first;
    DynamicBufferIterator m_last;

    CaptureList m_captures;
};

typedef std::vector<Selection> SelectionList;

class IncrementalInserter;
class HighlighterGroup;

// A Window is an editing view onto a Buffer
//
// The Window class manage a set of selections and provides means to modify
// both the selections and the buffer. It also handle the display of the
// buffer with it's highlighters.
class Window
{
public:
    typedef BufferString String;
    typedef std::function<Selection (const BufferIterator&)> Selector;
    typedef std::function<SelectionList (const Selection&)>  MultiSelector;

    void erase();
    void insert(const String& string);
    void append(const String& string);
    void replace(const String& string);

    const BufferCoord& position() const { return m_position; }
    DisplayCoord   cursor_position() const;
    BufferIterator cursor_iterator() const;

    Buffer& buffer() const { return m_buffer; }

    BufferIterator iterator_at(const DisplayCoord& window_pos) const;
    DisplayCoord   line_and_column_at(const BufferIterator& iterator) const;

    void move_selections(const DisplayCoord& offset, bool append = false);
    void move_cursor_to(const BufferIterator& iterator);

    void clear_selections();
    void keep_selection(int index);
    void select(const Selector& selector, bool append = false);
    void multi_select(const MultiSelector& selector);
    BufferString selection_content() const;
    const SelectionList& selections() const { return m_selections.back(); }

    void set_dimensions(const DisplayCoord& dimensions);

    const DisplayBuffer& display_buffer() const { return m_display_buffer; }

    void update_display_buffer();

    bool undo();
    bool redo();

    std::string status_line() const;

    struct id_not_unique : public runtime_error
    {
        id_not_unique(const std::string& id)
            : runtime_error("id not unique: " + id) {}
    };

    HighlighterGroup& highlighters() { return m_highlighters; }

    void add_filter(FilterAndId&& filter);
    void remove_filter(const std::string& id);

    CandidateList complete_filterid(const std::string& prefix,
                                    size_t cursor_pos = std::string::npos);

    void push_selections();
    void pop_selections();

    HooksManager& hooks_manager() { return m_hooks_manager; }

private:
    friend class Buffer;

    Window(Buffer& buffer);
    Window(const Window&) = delete;

    void check_invariant() const;
    void scroll_to_keep_cursor_visible_ifn();

    void erase_noundo();
    void insert_noundo(const String& string);
    void append_noundo(const String& string);

    SelectionList& selections() { return m_selections.back(); }

    friend class IncrementalInserter;
    IncrementalInserter* m_current_inserter;

    Buffer&       m_buffer;
    BufferCoord   m_position;
    DisplayCoord  m_dimensions;
    std::vector<SelectionList> m_selections;
    DisplayBuffer m_display_buffer;

    HighlighterGroup m_highlighters;
    idvaluemap<std::string, FilterFunc> m_filters;

    HooksManager     m_hooks_manager;
};

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

    IncrementalInserter(Window& window, Mode mode = Mode::Insert);
    ~IncrementalInserter();

    void insert(const Window::String& string);
    void insert_capture(size_t index);
    void erase();
    void move_cursors(const DisplayCoord& offset);

private:
    void apply(Modification&& modification) const;

    Window& m_window;
};

}

#endif // window_hh_INCLUDED
