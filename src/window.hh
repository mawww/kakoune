#ifndef window_hh_INCLUDED
#define window_hh_INCLUDED

#include "display_buffer.hh"
#include "highlighter_group.hh"
#include "option_manager.hh"
#include "safe_ptr.hh"
#include "scope.hh"

namespace Kakoune
{

// A Window is a view onto a Buffer
class Window : public SafeCountable, public OptionManagerWatcher, public Scope
{
public:
    Window(Buffer& buffer);
    ~Window();

    const CharCoord& position() const { return m_position; }
    void set_position(CharCoord position);

    const CharCoord& dimensions() const { return m_dimensions; }
    void set_dimensions(CharCoord dimensions);

    void scroll(LineCount offset);
    void center_line(LineCount buffer_line);
    void display_line_at(LineCount buffer_line, LineCount display_line);

    void scroll(CharCount offset);
    void center_column(CharCount buffer_column);
    void display_column_at(CharCount buffer_column, CharCount display_column);

    const DisplayBuffer& update_display_buffer(const Context& context);

    CharCoord display_position(ByteCoord coord) const;
    ByteCoord buffer_coord(CharCoord coord) const;

    Highlighter& highlighters() { return m_highlighters; }

    Buffer& buffer() const { return *m_buffer; }

    bool needs_redraw(const Context& context) const;
    void force_redraw() { m_hash = -1; }

    ByteCoord offset_coord(ByteCoord coord, CharCount offset);
    ByteCoordAndTarget offset_coord(ByteCoordAndTarget coord, LineCount offset);

    void clear_display_buffer();
private:
    Window(const Window&) = delete;

    void on_option_changed(const Option& option) override;
    void scroll_to_keep_selection_visible_ifn(const Context& context);

    void run_hook_in_own_context(StringView hook_name, StringView param);

    size_t compute_hash(const Context& context) const;

    SafePtr<Buffer> m_buffer;

    CharCoord m_position;
    CharCoord m_dimensions;
    DisplayBuffer m_display_buffer;

    HighlighterGroup m_highlighters;
    HighlighterGroup m_builtin_highlighters;

    // hash used to determine if a redraw is necessary
    size_t m_hash = -1;
};

}

#endif // window_hh_INCLUDED
