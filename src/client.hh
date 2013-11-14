#ifndef client_hh_INCLUDED
#define client_hh_INCLUDED

#include "editor.hh"
#include "string.hh"
#include "utils.hh"
#include "display_buffer.hh"
#include "input_handler.hh"

namespace Kakoune
{

class UserInterface;

class Client : public SafeCountable
{
public:
    Client(std::unique_ptr<UserInterface>&& ui, Editor& editor, String name);
    ~Client();

    // handle all the keys currently available in the user interface
    void handle_available_input();

    void print_status(DisplayLine status_line);

    void redraw_ifn();

    UserInterface& ui() const { return *m_ui; }

    void check_buffer_fs_timestamp();

    Context& context() { return m_input_handler.context(); }
    const Context& context() const { return m_input_handler.context(); }

private:
    InputHandler m_input_handler;

    DisplayLine generate_mode_line() const;

    std::unique_ptr<UserInterface> m_ui;

    DisplayLine m_status_line;
};

}

#endif // client_hh_INCLUDED
