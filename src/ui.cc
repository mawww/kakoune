#include "ui.hh"

#include "window.hh"

namespace Kakoune
{

UI* current_ui = nullptr;

void draw_editor_ifn(Editor& editor)
{
    Window* window = dynamic_cast<Window*>(&editor);
    if (current_ui and window)
        current_ui->draw_window(*window);
}

String prompt(const String& text, Completer completer)
{
    assert(current_ui);
    return current_ui->prompt(text, completer);
}

Key get_key()
{
    assert(current_ui);
    return current_ui->get_key();
}

void print_status(const String& status)
{
    assert(current_ui);
    return current_ui->print_status(status);
}

}
