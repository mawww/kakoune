#ifndef ui_hh_INCLUDED
#define ui_hh_INCLUDED

#include "keys.hh"
#include "completion.hh"

namespace Kakoune
{

class Editor;
class Window;
class String;

class UI
{
public:
    virtual ~UI() {}

    virtual void   draw_window(Window& window) = 0;
    virtual void   print_status(const String& status) = 0;
    virtual String prompt(const String& prompt, Completer completer) = 0;
    virtual Key    get_key() = 0;
};

struct prompt_aborted {};

extern UI* current_ui;

void draw_editor_ifn(Editor& editor);
String prompt(const String& text, Completer completer = complete_nothing);
Key get_key();
void print_status(const String& status);

}

#endif // ui_hh_INCLUDED

