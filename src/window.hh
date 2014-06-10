#ifndef window_hh_INCLUDED
#define window_hh_INCLUDED

#include "completion.hh"
#include "display_buffer.hh"
#include "highlighter_group.hh"
#include "selection.hh"
#include "hook_manager.hh"
#include "option_manager.hh"
#include "keymap_manager.hh"

namespace Kakoune
{

// A Window is a view onto a Buffer
class Window : public SafeCountable, public OptionManagerWatcher
{
public:
    Window(Buffer& buffer);
    ~Window();

    const CharCoord& position() const { return m_position; }
    void set_position(CharCoord position);

    const CharCoord& dimensions() const { return m_dimensions; }
    void set_dimensions(CharCoord dimensions);

    const DisplayBuffer& display_buffer() const { return m_display_buffer; }

    void center_line(LineCount buffer_line);
    void display_line_at(LineCount buffer_line, LineCount display_line);
    void scroll(LineCount offset);
    void scroll(CharCount offset);
    void update_display_buffer(const Context& context);

    CharCoord display_position(ByteCoord coord);

    HighlighterGroup& highlighters() { return m_highlighters; }

    OptionManager&       options()       { return m_options; }
    const OptionManager& options() const { return m_options; }
    HookManager&         hooks()         { return m_hooks; }
    const HookManager&   hooks()   const { return m_hooks; }
    KeymapManager&       keymaps()       { return m_keymaps; }
    const KeymapManager& keymaps() const { return m_keymaps; }

    Buffer& buffer() const { return *m_buffer; }

    size_t timestamp() const { return m_timestamp; }
    void   forget_timestamp() { m_timestamp = -1; }

    ByteCoord offset_coord(ByteCoord coord, CharCount offset);
    ByteCoord offset_coord(ByteCoord coord, LineCount offset);
private:
    Window(const Window&) = delete;

    void on_option_changed(const Option& option) override;
    void scroll_to_keep_selection_visible_ifn(const Context& context);

    safe_ptr<Buffer> m_buffer;

    CharCoord m_position;
    CharCoord m_dimensions;
    DisplayBuffer m_display_buffer;

    HookManager      m_hooks;
    OptionManager    m_options;
    KeymapManager    m_keymaps;

    HighlighterGroup m_highlighters;
    HighlighterGroup m_builtin_highlighters;

    size_t m_timestamp = -1;
};

}

#endif // window_hh_INCLUDED
