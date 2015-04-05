#ifndef ncurses_hh_INCLUDED
#define ncurses_hh_INCLUDED

#include "coord.hh"
#include "event_manager.hh"
#include "face.hh"
#include "user_interface.hh"
#include "array_view.hh"

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
              const DisplayLine& status_line,
              const DisplayLine& mode_line) override;

    bool   is_key_available() override;
    Key    get_key() override;

    void menu_show(ConstArrayView<String> items,
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
private:
    void check_resize();
    void redraw();
    void draw_line(const DisplayLine& line, CharCount col_index) const;

    NCursesWin* m_window = nullptr;

    CharCoord m_dimensions;
    void update_dimensions();

    NCursesWin* m_menu_win = nullptr;
    Vector<String> m_items;
    Face m_menu_fg;
    Face m_menu_bg;
    int m_selected_item = 0;
    int m_menu_columns = 1;
    LineCount m_menu_top_line = 0;
    void draw_menu();

    NCursesWin* m_info_win = nullptr;

    FDWatcher     m_stdin_watcher;
    InputCallback m_input_callback;

    bool m_status_on_top = false;
    ConstArrayView<StringView> m_assistant;

    int m_wheel_down_button = 2;
    int m_wheel_up_button = 4;

    bool m_dirty = false;
};

}

#endif // ncurses_hh_INCLUDED
