#ifndef client_manager_hh_INCLUDED
#define client_manager_hh_INCLUDED

#include "client.hh"
#include "completion.hh"

namespace Kakoune
{

struct WindowAndSelections
{
    std::unique_ptr<Window> window;
    SelectionList selections;
};

class ClientManager : public Singleton<ClientManager>
{
public:
    ClientManager();
    ~ClientManager();

    Client* create_client(std::unique_ptr<UserInterface>&& ui, int pid,
                          String name, EnvVarMap env_vars, StringView init_cmds,
                          Optional<BufferCoord> init_coord,
                          Client::OnExitCallback on_exit);

    bool   empty() const { return m_clients.empty(); }
    size_t count() const { return m_clients.size(); }

    void clear(bool exit);

    void ensure_no_client_uses_buffer(Buffer& buffer);

    WindowAndSelections get_free_window(Buffer& buffer);
    void add_free_window(std::unique_ptr<Window>&& window, SelectionList selections);

    void redraw_clients() const;
    bool process_pending_inputs();
    bool has_pending_inputs() const;

    Client*  get_client_ifp(StringView name);
    Client&  get_client(StringView name);
    bool client_name_exists(StringView name) const;
    void remove_client(Client& client, bool graceful, int status);

    using ClientList = Vector<std::unique_ptr<Client>, MemoryDomain::Client>;
    using iterator = ClientList::const_iterator;

    iterator begin() const { return m_clients.begin(); }
    iterator end() const { return m_clients.end(); }

    CandidateList complete_client_name(StringView name,
                                       ByteCount cursor_pos = -1) const;

    void clear_window_trash();
    void clear_client_trash();
private:
    String generate_name() const;

    ClientList m_clients;
    ClientList m_client_trash;
    Vector<WindowAndSelections, MemoryDomain::Client> m_free_windows;
    Vector<std::unique_ptr<Window>, MemoryDomain::Client> m_window_trash;
};

}

#endif // client_manager_hh_INCLUDED
