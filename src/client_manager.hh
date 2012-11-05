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
    void remove_client_by_context(Context& context);

    bool   empty() const { return m_clients.empty(); }
    size_t count() const { return m_clients.size(); }

    Window& get_unused_window_for_buffer(Buffer& buffer) const;

private:
    struct Client
    {
        Client(std::unique_ptr<UserInterface>&& ui, Window& window)
            : user_interface(std::move(ui)), input_handler(new InputHandler{}),
              context(new Context(*input_handler, window, *user_interface))
        {}

        Client(Client&&) = default;
        Client& operator=(Client&& other)
        {
             // drop safe pointers first
             context.reset();

             user_interface = std::move(other.user_interface);
             input_handler  = std::move(other.input_handler);
             context        = std::move(other.context);
             return *this;
        }

        std::unique_ptr<UserInterface> user_interface;
        std::unique_ptr<InputHandler>  input_handler;
        std::unique_ptr<Context>       context;
    };

    std::vector<Client> m_clients;
};

}

#endif // client_manager_hh_INCLUDED

