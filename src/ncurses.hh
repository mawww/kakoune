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

    void draw_window(Window& window) override;
    void print_status(const String& status, CharCount cursor_pos) override;

    Key    get_key() override;

    void menu_show(const memoryview<String>& choices,
                   const DisplayCoord& anchor, MenuStyle style) override;
    void menu_select(int selected) override;
    void menu_hide() override;
private:
    MENU* m_menu = nullptr;
    WINDOW* m_menu_win = nullptr;
    std::vector<ITEM*> m_items;
    std::vector<String> m_choices;

    DisplayCoord m_menu_pos;
    DisplayCoord m_menu_size;

    int m_menu_fg;
    int m_menu_bg;
};

}

#endif // ncurses_hh_INCLUDED

