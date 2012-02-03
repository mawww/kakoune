#ifndef window_hh_INCLUDED
#define window_hh_INCLUDED

#include <functional>

#include "editor.hh"
#include "display_buffer.hh"
#include "completion.hh"
#include "highlighter.hh"
#include "highlighter_group.hh"

namespace Kakoune
{
class HighlighterGroup;

// A Window is an editing view onto a Buffer
//
// The Window class is an interactive Editor adding display functionalities
// to the editing ones already provided by the Editor class.
// Display can be customized through the use of highlighters handled by
// the window's HighlighterGroup
class Window : public Editor
{
public:
    const BufferCoord& position() const { return m_position; }

    DisplayCoord   cursor_position() const;
    BufferIterator cursor_iterator() const;

    BufferIterator iterator_at(const DisplayCoord& window_pos) const;
    DisplayCoord   line_and_column_at(const BufferIterator& iterator) const;

    void set_dimensions(const DisplayCoord& dimensions);

    const DisplayBuffer& display_buffer() const { return m_display_buffer; }

    void update_display_buffer();

    const SelectionList& selections() const { return Editor::selections(); }

    std::string status_line() const;

    HighlighterGroup& highlighters() { return m_highlighters; }

    HooksManager& hooks_manager() { return m_hooks_manager; }

private:
    friend class Buffer;

    Window(Buffer& buffer);
    Window(const Window&) = delete;

    void on_end_batch();

    void scroll_to_keep_cursor_visible_ifn();

    BufferCoord   m_position;
    DisplayCoord  m_dimensions;
    DisplayBuffer m_display_buffer;

    HighlighterGroup m_highlighters;

    HooksManager     m_hooks_manager;
};

}

#endif // window_hh_INCLUDED
