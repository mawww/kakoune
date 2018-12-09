#ifndef ncurses_hh_INCLUDED
#define ncurses_hh_INCLUDED

#include "array_view.hh"
#include "coord.hh"
#include "event_manager.hh"
#include "face.hh"
#include "hash_map.hh"
#include "optional.hh"
#include "string.hh"
#include "user_interface.hh"

namespace Kakoune
{

struct NCursesWin;

class NCursesUI : public UserInterface
{
public:
    NCursesUI();
    ~NCursesUI() override;

    NCursesUI(const NCursesUI&) = delete;
    NCursesUI& operator=(const NCursesUI&) = delete;

    bool is_ok() const override { return m_window != nullptr; }

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

    void info_show(StringView title, StringView content,
                   DisplayCoord anchor, Face face,
                   InfoStyle style) override;
    void info_hide() override;

    void set_cursor(CursorMode mode, DisplayCoord coord) override;

    void refresh(bool force) override;

    DisplayCoord dimensions() override;
    void set_on_key(OnKeyCallback callback) override;
    void set_ui_options(const Options& options) override;

    static void abort();

    struct Rect
    {
        DisplayCoord pos;
        DisplayCoord size;
    };

private:
    void check_resize(bool force = false);
    void redraw();

    int get_color(Color color);
    int get_color_pair(const Face& face);
    void set_face(NCursesWin* window, Face face, const Face& default_face);
    void draw_line(NCursesWin* window, const DisplayLine& line,
                   ColumnCount col_index, ColumnCount max_column,
                   const Face& default_face);

    Optional<Key> get_next_key();

    NCursesWin* m_window = nullptr;

    DisplayCoord m_dimensions;

    using ColorPair = std::pair<Color, Color>;
    HashMap<Color, int, MemoryDomain::Faces> m_colors;
    HashMap<ColorPair, int, MemoryDomain::Faces> m_colorpairs;
    int m_next_color = 16;
    int m_next_pair = 1;
    int m_active_pair = -1;

    struct Window : Rect
    {
        void create(const DisplayCoord& pos, const DisplayCoord& size);
        void destroy();
        void refresh();

        explicit operator bool() const { return win; }

        NCursesWin* win = nullptr;
    };

    void mark_dirty(const Window& win);

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
        String title;
        String content;
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

    static constexpr int default_shift_function_key = 12;
    int m_shift_function_key = default_shift_function_key;

    bool m_set_title = true;
    bool m_change_colors = true;
    bool m_builtin_key_parser = false;

    bool m_dirty = false;

    bool m_resize_pending = false;
    void set_resize_pending();

    ColumnCount m_status_len = 0;
};

}

#endif // ncurses_hh_INCLUDED
