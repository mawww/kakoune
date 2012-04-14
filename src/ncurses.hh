#ifndef ncurses_hh_INCLUDED
#define ncurses_hh_INCLUDED

#include <functional>

#include "keys.hh"
#include "completion.hh"

namespace Kakoune
{

class Window;

typedef std::function<String (const String&, Completer)> PromptFunc;
typedef std::function<Key ()> GetKeyFunc;

struct prompt_aborted {};

namespace NCurses
{

void init(PromptFunc& prompt_func, GetKeyFunc& get_key_func);
void deinit();
void draw_window(Window& window);
void print_status(const String& status);

}

}

#endif // ncurses_hh_INCLUDED

