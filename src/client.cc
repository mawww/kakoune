#include "client.hh"

#include "window.hh"

namespace Kakoune
{

Client* current_client = nullptr;

void draw_editor_ifn(Editor& editor)
{
    Window* window = dynamic_cast<Window*>(&editor);
    if (current_client and window)
        current_client->draw_window(*window);
}

String prompt(const String& text, Completer completer)
{
    assert(current_client);
    return current_client->prompt(text, completer);
}

Key get_key()
{
    assert(current_client);
    return current_client->get_key();
}

void print_status(const String& status)
{
    assert(current_client);
    return current_client->print_status(status);
}

}
