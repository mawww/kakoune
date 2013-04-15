#ifndef client_manager_hh_INCLUDED
#define client_manager_hh_INCLUDED

#include "context.hh"
#include "input_handler.hh"

namespace Kakoune
{

struct client_removed{};

class Client
{
public:
    Context& context() { return m_input_handler.context(); }
    const String& name() const { return m_name; }

private:
    friend class ClientManager;

    Client(std::unique_ptr<UserInterface>&& ui,
           Window& window, String name);
    Client(Client&&) = delete;
    Client& operator=(Client&& other) = delete;

    const std::unique_ptr<UserInterface> m_user_interface;
    InputHandler  m_input_handler;
    String        m_name;
};


class ClientManager : public Singleton<ClientManager>
{
public:
    ClientManager();
    ~ClientManager();

    Client* create_client(std::unique_ptr<UserInterface>&& ui,
                          const String& init_cmd);

    bool   empty() const { return m_clients.empty(); }
    size_t count() const { return m_clients.size(); }

    Window& get_unused_window_for_buffer(Buffer& buffer);
    void    ensure_no_client_uses_buffer(Buffer& buffer);

    void redraw_clients() const;

    Client&  get_client(const Context& context);
    Client&  get_client(const String& name);
    void set_client_name(Client& client, String name);
    void remove_client(Client& client);

private:
    String generate_name() const;

    std::vector<std::unique_ptr<Client>> m_clients;
    std::vector<std::unique_ptr<Window>> m_windows;
};

}

#endif // client_manager_hh_INCLUDED

