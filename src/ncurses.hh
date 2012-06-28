#ifndef ncurses_hh_INCLUDED
#define ncurses_hh_INCLUDED

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

    String prompt(const String& prompt, Completer completer);
    Key    get_key();
};

}

#endif // ncurses_hh_INCLUDED

