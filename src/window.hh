#ifndef window_hh_INCLUDED
#define window_hh_INCLUDED

#include "completion.hh"
#include "display_buffer.hh"
#include "editor.hh"
#include "highlighter.hh"
#include "highlighter.hh"
#include "hook_manager.hh"
#include "option_manager.hh"

namespace Kakoune
{

// A Window is an editing view onto a Buffer
//
// The Window class is an interactive Editor adding display functionalities
// to the editing ones already provided by the Editor class.
// Display can be customized through the use of highlighters handled by
// the window's HighlighterGroup
class Window : public Editor, public OptionManagerWatcher
{
public:
    Window(Buffer& buffer);
    ~Window();

    const DisplayCoord& position() const { return m_position; }
    void set_position(const DisplayCoord& position);

    const DisplayCoord& dimensions() const { return m_dimensions; }
    void set_dimensions(const DisplayCoord& dimensions);

    const DisplayBuffer& display_buffer() const { return m_display_buffer; }

    void center_selection();
    void display_selection_at(LineCount line);
    void scroll(LineCount offset);
    void update_display_buffer();

    DisplayCoord display_position(const BufferIterator& it);

    HighlighterGroup& highlighters() { return m_highlighters; }

    OptionManager&       options()       { return m_options; }
    const OptionManager& options() const { return m_options; }
    HookManager&         hooks()         { return m_hooks; }
    const HookManager&   hooks()   const { return m_hooks; }

    size_t timestamp() const { return m_timestamp; }
    void   forget_timestamp() { m_timestamp = -1; }

private:
    Window(const Window&) = delete;

    void on_option_changed(const Option& option) override;

    void scroll_to_keep_cursor_visible_ifn();

    DisplayCoord  m_position;
    DisplayCoord  m_dimensions;
    DisplayBuffer m_display_buffer;

    HookManager      m_hooks;
    OptionManager    m_options;

    HighlighterGroup m_highlighters;
    HighlighterGroup m_builtin_highlighters;

    size_t m_timestamp = -1;
};

}

#endif // window_hh_INCLUDED
