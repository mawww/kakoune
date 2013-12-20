#ifndef client_manager_hh_INCLUDED
#define client_manager_hh_INCLUDED

#include "client.hh"

namespace Kakoune
{

struct client_removed{};

using WindowAndSelections = std::tuple<std::unique_ptr<Window>,
                                       DynamicSelectionList>;

class ClientManager : public Singleton<ClientManager>
{
public:
    ClientManager();
    ~ClientManager();

    Client* create_client(std::unique_ptr<UserInterface>&& ui,
                          const String& init_cmd);

    bool   empty() const { return m_clients.empty(); }
    size_t count() const { return m_clients.size(); }

    void    ensure_no_client_uses_buffer(Buffer& buffer);

    WindowAndSelections get_free_window(Buffer& buffer);
    void add_free_window(std::unique_ptr<Window>&& window, SelectionList selections);

    void redraw_clients() const;

    Client*  get_client_ifp(const String& name);
    Client&  get_client(const String& name);
    bool validate_client_name(const String& name) const;
    void remove_client(Client& client);

private:
    String generate_name() const;

    std::vector<std::unique_ptr<Client>> m_clients;
    std::vector<WindowAndSelections> m_free_windows;
};

}

#endif // client_manager_hh_INCLUDED

