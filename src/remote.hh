#ifndef remote_hh_INCLUDED
#define remote_hh_INCLUDED

#include "user_interface.hh"
#include "display_buffer.hh"
#include "event_manager.hh"

namespace Kakoune
{

struct peer_disconnected {};

void handle_remote(FDWatcher& event);

class RemoteClient
{
public:
    RemoteClient(int socket, UserInterface* ui,
                 const String& init_command);

    void process_next_message();
    void write_next_key();

private:
    std::unique_ptr<UserInterface> m_ui;
    DisplayCoord                   m_dimensions;
    FDWatcher                      m_socket_watcher;
};

}

#endif // remote_hh_INCLUDED

