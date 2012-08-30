#ifndef client_hh_INCLUDED
#define client_hh_INCLUDED

#include "keys.hh"
#include "completion.hh"
#include "utils.hh"

namespace Kakoune
{

class Editor;
class Window;
class String;
class Context;

enum class MenuCommand
{
    SelectPrev,
    SelectNext,
    Close,
};

class Client : public SafeCountable
{
public:
    virtual ~Client() {}

    virtual void   draw_window(Window& window) = 0;
    virtual void   print_status(const String& status) = 0;
    virtual String prompt(const String& prompt, const Context& context,
                          Completer completer = complete_nothing) = 0;
    virtual Key    get_key() = 0;

    virtual void   show_menu(const memoryview<String>& choices) = 0;
    virtual void   menu_ctrl(MenuCommand command) = 0;
};

struct prompt_aborted {};

}

#endif // client_hh_INCLUDED
