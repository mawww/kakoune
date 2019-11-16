#ifndef terminal_hh_INCLUDED
#define terminal_hh_INCLUDED

#include "array_view.hh"
#include "coord.hh"
#include "display_buffer.hh"
#include "event_manager.hh"
#include "face.hh"
#include "hash_map.hh"
#include "optional.hh"
#include "string.hh"
#include "user_interface.hh"

#include <termios.h>

namespace Kakoune
{

struct DisplayAtom;

class TerminalUI : public UserInterface, public Singleton<TerminalUI>
{
public:
    TerminalUI();
    ~TerminalUI() override;

    TerminalUI(const TerminalUI&) = delete;
    TerminalUI& operator=(const TerminalUI&) = delete;

    bool is_ok() const override { return (bool)m_window; }

    void draw(const DisplayBuffer& display_buffer,
              const Face& default_face,
              const Face& padding_face) override;

    void draw_status(const DisplayLine& status_line,
                     const DisplayLine& mode_line,
                     const Face& default_face) override;

    void menu_show(ConstArrayView<DisplayLine> items,
                   DisplayCoord anchor, Face fg, Face bg,
                   MenuStyle style) override;
    void menu_select(int selected) override;
    void menu_hide() override;

    void info_show(const DisplayLine& title, const DisplayLineList& content,
                   DisplayCoord anchor, Face face,
                   InfoStyle style) override;
    void info_hide() override;

    void set_cursor(CursorMode mode, DisplayCoord coord) override;

    void refresh(bool force) override;

    DisplayCoord dimensions() override;
    void set_on_key(OnKeyCallback callback) override;
    void set_ui_options(const Options& options) override;

    static void setup_terminal();
    static void restore_terminal();

    void suspend();

    struct Rect
    {
        DisplayCoord pos;
        DisplayCoord size;
    };

private:
    void check_resize(bool force = false);
    void redraw(bool force);

    Optional<Key> get_next_key();

    struct Window : Rect
    {
        void create(const DisplayCoord& pos, const DisplayCoord& size);
        void destroy();
        void refresh(bool force);
        void move_cursor(DisplayCoord coord);
        void draw(ConstArrayView<DisplayAtom> atoms, const Face& default_face);

        explicit operator bool() const { return not lines.empty(); }

        struct Atom
        {
            String text;
            Face face;
        };
        Vector<Vector<Atom>> lines;
        DisplayCoord cursor;

        void clear_line();
    };

    Window m_window;

    DisplayCoord m_dimensions;
    termios m_original_termios{};

    void set_raw_mode() const;

    struct Menu : Window
    {
        Vector<DisplayLine, MemoryDomain::Display> items;
        Face fg;
        Face bg;
        DisplayCoord anchor;
        MenuStyle style;
        int selected_item = 0;
        int first_item = 0;
        int columns = 1;
    } m_menu;

    void draw_menu();

    LineCount content_line_offset() const;

    struct Info : Window
    {
        DisplayLine title;
        DisplayLineList content;
        Face face;
        DisplayCoord anchor;
        InfoStyle style;
    } m_info;

    struct Cursor
    {
        CursorMode mode;
        DisplayCoord coord;
    } m_cursor;

    FDWatcher m_stdin_watcher;
    OnKeyCallback m_on_key;

    bool m_status_on_top = false;
    ConstArrayView<StringView> m_assistant;

    void enable_mouse(bool enabled);

    bool m_mouse_enabled = false;
    int m_wheel_up_button = 4;
    int m_wheel_down_button = 5;
    int m_wheel_scroll_amount = 3;
    int m_mouse_state = 0;

    static constexpr int default_shift_function_key = 12;
    int m_shift_function_key = default_shift_function_key;

    bool m_set_title = true;

    bool m_dirty = false;

    bool m_resize_pending = false;
    void set_resize_pending();

    ColumnCount m_status_len = 0;
};

}

#endif // terminal_hh_INCLUDED
