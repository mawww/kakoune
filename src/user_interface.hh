#ifndef user_interface_hh_INCLUDED
#define user_interface_hh_INCLUDED

#include "memoryview.hh"
#include "keys.hh"
#include "units.hh"

namespace Kakoune
{

class String;
class Window;

class UserInterface
{
public:
    virtual ~UserInterface() {}
    virtual void print_status(const String& status, CharCount cursor_pos = -1) = 0;
    virtual void menu_show(const memoryview<String>& choices) = 0;
    virtual void menu_select(int selected) = 0;
    virtual void menu_hide() = 0;
    virtual void draw_window(Window& window) = 0;
    virtual Key  get_key() = 0;
};

}

#endif // user_interface_hh_INCLUDED
