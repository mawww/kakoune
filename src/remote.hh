#ifndef remote_hh_INCLUDED
#define remote_hh_INCLUDED

#include "env_vars.hh"
#include "exception.hh"
#include "session_manager.hh"
#include "utils.hh"
#include "vector.hh"
#include "optional.hh"

#include <memory>

namespace Kakoune
{

class FDWatcher;
class UserInterface;

template<typename T> struct Optional;
struct BufferCoord;

using RemoteBuffer = Vector<char, MemoryDomain::Remote>;

// A remote client handle communication between a client running on the server
// and a user interface running on the local process.
class RemoteClient
{
public:
    RemoteClient(StringView session, StringView name, std::unique_ptr<UserInterface>&& ui,
                 int pid, const EnvVarMap& env_vars, StringView init_command,
                 Optional<BufferCoord> init_coord);

    bool is_ui_ok() const;
    const Optional<int>& exit_status() const { return m_exit_status; }
private:
    std::unique_ptr<UserInterface> m_ui;
    std::unique_ptr<FDWatcher>     m_socket_watcher;
    RemoteBuffer                   m_send_buffer;
    Optional<int>                  m_exit_status;
};

void send_command(StringView session, StringView command);

struct Server : public Singleton<Server>
{
    Server();
    ~Server();

    void close_session(bool do_unlink = true);

    bool negotiating() const { return not m_accepters.empty(); }

private:
    class Accepter;
    void remove_accepter(Accepter* accepter);

    std::unique_ptr<FDWatcher> m_listener;
    Vector<std::unique_ptr<Accepter>, MemoryDomain::Remote> m_accepters;
};

}

#endif // remote_hh_INCLUDED
