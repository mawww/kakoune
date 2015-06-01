#ifndef remote_hh_INCLUDED
#define remote_hh_INCLUDED

#include "coord.hh"
#include "env_vars.hh"
#include "exception.hh"
#include "user_interface.hh"
#include "utils.hh"

#include <memory>

namespace Kakoune
{

struct peer_disconnected {};

struct connection_failed : runtime_error
{
    connection_failed(StringView filename)
        : runtime_error{format("connect to {} failed", filename)}
    {}
};

class FDWatcher;

// A remote client handle communication between a client running on the server
// and a user interface running on the local process.
class RemoteClient
{
public:
    RemoteClient(StringView session, std::unique_ptr<UserInterface>&& ui,
                 const EnvVarMap& env_vars, StringView init_command);

private:
    void process_available_messages();
    void process_next_message();
    void write_next_key();

    std::unique_ptr<UserInterface> m_ui;
    std::unique_ptr<FDWatcher>     m_socket_watcher;
    CharCoord                      m_dimensions;
};

void send_command(StringView session, StringView command);

struct Server : public Singleton<Server>
{
    Server(String session_name);
    ~Server();
    const String& session() const { return m_session; }

    void close_session();

private:
    class Accepter;
    void remove_accepter(Accepter* accepter);

    String m_session;
    std::unique_ptr<FDWatcher> m_listener;
    Vector<std::unique_ptr<Accepter>> m_accepters;
};

}

#endif // remote_hh_INCLUDED
