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
    void create_client(std::unique_ptr<UserInterface>&& ui,
                       Buffer& buffer, int event_fd);

    bool   empty() const { return m_clients.empty(); }
    size_t count() const { return m_clients.size(); }

    Window& get_unused_window_for_buffer(Buffer& buffer);
    void    ensure_no_client_uses_buffer(Buffer& buffer);

    void redraw_clients() const;
private:
    void remove_client_by_context(Context& context);

    struct Client
    {
        Client(std::unique_ptr<UserInterface>&& ui, Window& window)
            : user_interface(std::move(ui)),
              context(input_handler, window, *user_interface) {}
        Client(Client&&) = delete;
        Client& operator=(Client&& other) = delete;

        std::unique_ptr<UserInterface> user_interface;
        InputHandler  input_handler;
        Context       context;
    };

    std::vector<std::unique_ptr<Client>> m_clients;
    std::vector<std::unique_ptr<Window>> m_windows;
};

}

#endif // client_manager_hh_INCLUDED

