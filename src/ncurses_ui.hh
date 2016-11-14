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
              const Face& default_face,
              const Face& padding_face) override;

    void draw_status(const DisplayLine& status_line,
                     const DisplayLine& mode_line,
                     const Face& default_face) override;

    bool is_key_available() override;
    Key  get_key() override;

    void menu_show(ConstArrayView<DisplayLine> items,
                   DisplayCoord anchor, Face fg, Face bg,
                   MenuStyle style) override;
    void menu_select(int selected) override;
    void menu_hide() override;

    void info_show(StringView title, StringView content,
                   DisplayCoord anchor, Face face,
                   InfoStyle style) override;
    void info_hide() override;

    void refresh(bool force) override;

    void set_input_callback(InputCallback callback) override;

    void set_ui_options(const Options& options) override;

    DisplayCoord dimensions() override;

    static void abort();

    struct Rect
    {
        DisplayCoord pos;
        DisplayCoord size;
    };

protected:
    void on_sighup();

private:
    void check_resize(bool force = false);
    void redraw();

    int get_color(Color color);
    int get_color_pair(const Face& face);
    void set_face(NCursesWin* window, Face face, const Face& default_face);
    void draw_line(NCursesWin* window, const DisplayLine& line,
                   ColumnCount col_index, ColumnCount max_column,
                   const Face& default_face);

    NCursesWin* m_window = nullptr;

    DisplayCoord m_dimensions;

    using ColorPair = std::pair<Color, Color>;
    UnorderedMap<Color, int, MemoryDomain::Faces> m_colors;
    UnorderedMap<ColorPair, int, MemoryDomain::Faces> m_colorpairs;
    int m_next_color = 16;

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
        Vector<DisplayLine> items;
        Face fg;
        Face bg;
        DisplayCoord anchor;
        MenuStyle style;
        int selected_item = 0;
        int columns = 1;
        LineCount top_line = 0;
    } m_menu;

    void draw_menu();

    struct Info : Window
    {
        String title;
        String content;
        Face face;
        DisplayCoord anchor;
        InfoStyle style;
    } m_info;

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
