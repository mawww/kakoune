#ifndef ncurses_hh_INCLUDED
#define ncurses_hh_INCLUDED

#include <ncurses.h>

#include "user_interface.hh"
#include "display_buffer.hh"
#include "event_manager.hh"

namespace Kakoune
{

class NCursesUI : public UserInterface
{
public:
    NCursesUI();
    ~NCursesUI();

    NCursesUI(const NCursesUI&) = delete;
    NCursesUI& operator=(const NCursesUI&) = delete;

    void draw(const DisplayBuffer& display_buffer,
              const String& mode_line) override;
    void print_status(const String& status, CharCount cursor_pos) override;

    bool   is_key_available() override;
    Key    get_key() override;

    void menu_show(const memoryview<String>& choices,
                   DisplayCoord anchor, ColorPair fg, ColorPair bg,
                   MenuStyle style) override;
    void menu_select(int selected) override;
    void menu_hide() override;

    void info_show(const String& content, DisplayCoord anchor,
                   ColorPair colors, MenuStyle style) override;
    void info_hide() override;

    void set_input_callback(InputCallback callback) override;

    DisplayCoord dimensions() override;
private:
    friend void on_term_resize(int);
    void redraw();

    DisplayCoord m_dimensions;
    void update_dimensions();

    String    m_status_line;
    CharCount m_status_cursor = -1;
    void      draw_status();

    WINDOW* m_menu_win = nullptr;
    std::vector<String> m_choices;
    ColorPair m_menu_fg;
    ColorPair m_menu_bg;
    int m_selected_choice = 0;
    int m_menu_columns = 1;
    LineCount m_menu_top_line = 0;
    void draw_menu();

    WINDOW* m_info_win = nullptr;

    FDWatcher     m_stdin_watcher;
    InputCallback m_input_callback;
};

}

#endif // ncurses_hh_INCLUDED

