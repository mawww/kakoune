#ifndef client_manager_hh_INCLUDED
#define client_manager_hh_INCLUDED

#include "context.hh"
#include "input_handler.hh"

namespace Kakoune
{

struct client_removed{};

class ClientManager : public Singleton<ClientManager>
{
public:
    ClientManager();
    ~ClientManager();

    InputHandler* create_client(std::unique_ptr<UserInterface>&& ui,
                                const String& init_cmd);

    bool   empty() const { return m_clients.empty(); }
    size_t count() const { return m_clients.size(); }

    Window& get_unused_window_for_buffer(Buffer& buffer);
    void    ensure_no_client_uses_buffer(Buffer& buffer);

    void redraw_clients() const;

    InputHandler& get_client(const Context& context);
    InputHandler&  get_client(const String& name);
    void set_client_name(InputHandler& client, String name);
    void remove_client(InputHandler& client);

private:
    String generate_name() const;

    std::vector<std::unique_ptr<InputHandler>> m_clients;
    std::vector<std::unique_ptr<Window>> m_windows;
};

}

#endif // client_manager_hh_INCLUDED

