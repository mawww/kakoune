#ifndef window_hh_INCLUDED
#define window_hh_INCLUDED

#include "client.hh"
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
    ~Window() override;

    const DisplayCoord& position() const { return m_position; }
    void set_position(DisplayCoord position);

    const DisplayCoord& dimensions() const { return m_dimensions; }
    void set_dimensions(DisplayCoord dimensions);

    void scroll(LineCount offset);
    void center_line(LineCount buffer_line);
    void display_line_at(LineCount buffer_line, LineCount display_line);

    void scroll(ColumnCount offset);
    void center_column(ColumnCount buffer_column);
    void display_column_at(ColumnCount buffer_column, ColumnCount display_column);

    const DisplayBuffer& update_display_buffer(const Context& context);

    DisplayCoord display_position(BufferCoord coord) const;
    BufferCoord buffer_coord(DisplayCoord coord) const;

    Highlighter& highlighters() { return m_highlighters; }

    Buffer& buffer() const { return *m_buffer; }

    bool needs_redraw(const Context& context) const;
    void force_redraw() { m_last_setup = Setup{}; }

    BufferCoord offset_coord(BufferCoord coord, CharCount offset);
    BufferCoordAndTarget offset_coord(BufferCoordAndTarget coord, LineCount offset);

    void set_client(Client* client) { m_client = client; }

    void clear_display_buffer();
private:
    Window(const Window&) = delete;

    void on_option_changed(const Option& option) override;
    void scroll_to_keep_selection_visible_ifn(const Context& context);

    void run_hook_in_own_context(StringView hook_name, StringView param,
                                 String client_name = "");

    SafePtr<Buffer> m_buffer;
    SafePtr<Client> m_client;

    DisplayCoord m_position;
    DisplayCoord m_dimensions;
    DisplayBuffer m_display_buffer;

    HighlighterGroup m_highlighters;
    HighlighterGroup m_builtin_highlighters;

    struct Setup
    {
        DisplayCoord position;
        DisplayCoord dimensions;
        size_t timestamp;
        size_t main_selection;
        Vector<BufferRange, MemoryDomain::Display> selections;
    };
    Setup build_setup(const Context& context) const;
    Setup m_last_setup;
};

}

#endif // window_hh_INCLUDED
