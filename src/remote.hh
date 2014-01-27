#ifndef remote_hh_INCLUDED
#define remote_hh_INCLUDED

#include "display_buffer.hh"
#include "event_manager.hh"
#include "user_interface.hh"

namespace Kakoune
{

struct peer_disconnected {};

// A remote client handle communication between a client running on the server
// and a user interface running on the local process.
class RemoteClient
{
public:
    RemoteClient(int socket, std::unique_ptr<UserInterface>&& ui,
                 const String& init_command);

private:
    void process_next_message();
    void write_next_key();

    std::unique_ptr<UserInterface> m_ui;
    DisplayCoord                   m_dimensions;
    FDWatcher                      m_socket_watcher;
};
std::unique_ptr<RemoteClient> connect_to(const String& pid,
                                         std::unique_ptr<UserInterface>&& ui,
                                         const String& init_command);

struct Server : public Singleton<Server>
{
    Server(String session_name);
    ~Server();
    const String& session() const { return m_session; }

    void close_session();

private:
    String m_session;
    std::unique_ptr<FDWatcher> m_listener;
};

}

#endif // remote_hh_INCLUDED

