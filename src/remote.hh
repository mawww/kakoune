#ifndef remote_hh_INCLUDED
#define remote_hh_INCLUDED

#include "user_interface.hh"
#include "display_buffer.hh"
#include "event_manager.hh"

namespace Kakoune
{

struct peer_disconnected {};

// A client accepter handle a connection until it closes or a nul byte is
// recieved. Everything recieved before is considered to be a command.
//
// * When a nul byte is recieved, the socket is handed to a new Client along
//   with the command.
// * When the connection is closed, the command is run in an empty context.
class ClientAccepter
{
public:
    ClientAccepter(int socket);
private:
    void handle_available_input();

    String    m_buffer;
    FDWatcher m_socket_watcher;
};

// A remote client handle communication between a client running on the server
// and a user interface running on the local process.
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

