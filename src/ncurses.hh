#ifndef ncurses_hh_INCLUDED
#define ncurses_hh_INCLUDED

#include <ncurses.h>
#include <menu.h>

#include "client.hh"

namespace Kakoune
{

class NCursesClient : public Client
{
public:
    NCursesClient();
    ~NCursesClient();

    NCursesClient(const NCursesClient&) = delete;
    NCursesClient& operator=(const NCursesClient&) = delete;

    void draw_window(Window& window);
    void print_status(const String& status);

    String prompt(const String& prompt, const Context& context, Completer completer);
    Key    get_key();

    void show_menu(const memoryview<String>& choices);
    void menu_ctrl(MenuCommand command);
private:
    MENU* m_menu;
    std::vector<ITEM*> m_items;
    std::vector<String> m_counts;
    std::vector<String> m_choices;
};

}

#endif // ncurses_hh_INCLUDED

