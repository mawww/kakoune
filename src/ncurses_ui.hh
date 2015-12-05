#ifndef ncurses_hh_INCLUDED
#define ncurses_hh_INCLUDED

#include "coord.hh"
#include "event_manager.hh"
#include "face.hh"
#include "user_interface.hh"
#include "array_view.hh"
#include "unordered_map.hh"

namespace Kakoune
{

struct NCursesWin;

class NCursesUI : public UserInterface
{
public:
    NCursesUI();
    ~NCursesUI();

    NCursesUI(const NCursesUI&) = delete;
    NCursesUI& operator=(const NCursesUI&) = delete;

    void draw(const DisplayBuffer& display_buffer,
              const Face& default_face) override;

    void draw_status(const DisplayLine& status_line,
                     const DisplayLine& mode_line,
                     const Face& default_face) override;

    bool is_key_available() override;
    Key  get_key() override;

    void menu_show(ConstArrayView<DisplayLine> items,
                   CharCoord anchor, Face fg, Face bg,
                   MenuStyle style) override;
    void menu_select(int selected) override;
    void menu_hide() override;

    void info_show(StringView title, StringView content,
                   CharCoord anchor, Face face,
                   InfoStyle style) override;
    void info_hide() override;

    void refresh() override;

    void set_input_callback(InputCallback callback) override;

    void set_ui_options(const Options& options) override;

    CharCoord dimensions() override;

    static void abort();

    struct Rect
    {
        CharCoord pos;
        CharCoord size;
    };
private:
    void check_resize(bool force = false);
    void redraw();

    int get_color(Color color);
    int get_color_pair(const Face& face);
    void set_face(NCursesWin* window, Face face, const Face& default_face);
    void draw_line(NCursesWin* window, const DisplayLine& line,
                   CharCount col_index, CharCount max_column,
                   const Face& default_face);

    NCursesWin* m_window = nullptr;

    CharCoord m_dimensions;

    using ColorPair = std::pair<Color, Color>;
    UnorderedMap<Color, int, MemoryDomain::Faces> m_colors;
    UnorderedMap<ColorPair, int, MemoryDomain::Faces> m_colorpairs;
    int m_next_color = 16;

    struct Window : Rect
    {
        void create(const CharCoord& pos, const CharCoord& size);
        void destroy();
        void refresh();

        explicit operator bool() const { return win; }

        NCursesWin* win = nullptr;
    };

    void mark_dirty(const Window& win);

    Window m_menu;
    Vector<DisplayLine> m_items;
    Face m_menu_fg;
    Face m_menu_bg;
    CharCoord m_menu_anchor;
    MenuStyle m_menu_style;
    int m_selected_item = 0;
    int m_menu_columns = 1;
    LineCount m_menu_top_line = 0;
    void draw_menu();

    Window m_info;
    String m_info_title;
    String m_info_content;
    Face m_info_face;
    CharCoord m_info_anchor;
    InfoStyle m_info_style;

    FDWatcher     m_stdin_watcher;
    InputCallback m_input_callback;

    bool m_status_on_top = false;
    ConstArrayView<StringView> m_assistant;

    void enable_mouse(bool enabled);

    bool m_mouse_enabled = false;
    int m_wheel_up_button = 4;
    int m_wheel_down_button = 5;

    bool m_set_title = true;

    bool m_dirty = false;
};

}

#endif // ncurses_hh_INCLUDED
