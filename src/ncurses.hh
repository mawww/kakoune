#ifndef ncurses_hh_INCLUDED
#define ncurses_hh_INCLUDED

#include "ui.hh"

namespace Kakoune
{

class NCursesUI : public UI
{
public:
    NCursesUI();
    ~NCursesUI();

    NCursesUI(const NCursesUI&) = delete;
    NCursesUI& operator=(const NCursesUI&) = delete;

    void draw_window(Window& window);
    void print_status(const String& status);

    String prompt(const String& prompt, Completer completer);
    Key    get_key();
};

}

#endif // ncurses_hh_INCLUDED

