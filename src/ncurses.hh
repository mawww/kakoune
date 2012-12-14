#ifndef ncurses_hh_INCLUDED
#define ncurses_hh_INCLUDED

#include <ncurses.h>
#include <menu.h>

#include "user_interface.hh"
#include "display_buffer.hh"

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
                   const DisplayCoord& anchor, MenuStyle style) override;
    void menu_select(int selected) override;
    void menu_hide() override;

    void info_show(const String& content,
                   const DisplayCoord& anchor, MenuStyle style) override;
    void info_hide() override;

    DisplayCoord dimensions() override;
private:
    friend void on_term_resize(int);

    DisplayCoord m_dimensions;
    void update_dimensions();

    String    m_status_line;
    CharCount m_status_cursor = -1;
    void      draw_status();

    MENU* m_menu = nullptr;
    WINDOW* m_menu_win = nullptr;
    std::vector<ITEM*> m_items;
    std::vector<String> m_choices;

    int m_menu_fg;
    int m_menu_bg;

    WINDOW* m_info_win = nullptr;
};

}

#endif // ncurses_hh_INCLUDED

