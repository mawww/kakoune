#ifndef client_manager_hh_INCLUDED
#define client_manager_hh_INCLUDED

#include "context.hh"
#include "input_handler.hh"

namespace Kakoune
{

struct Client
{
    std::unique_ptr<UserInterface> ui;
    std::unique_ptr<InputHandler>  input_handler;
    std::unique_ptr<Context>       context;

    Client(UserInterface* ui, Window& window)
        : ui(ui),
          input_handler(new InputHandler{}),
          context(new Context(*input_handler, window, *ui)) {}

    Client(Client&&) = default;
    Client& operator=(Client&&) = default;
};

struct client_removed{};

class ClientManager : public Singleton<ClientManager>
{
public:
    void add_client(Client&& client);
    void remove_client_by_context(Context& context);

    bool   empty() const { return m_clients.empty(); }
    size_t count() const { return m_clients.size(); }
private:
    std::vector<Client> m_clients;
};

}

#endif // client_manager_hh_INCLUDED

