#ifndef remote_hh_INCLUDED
#define remote_hh_INCLUDED

#include "env_vars.hh"
#include "exception.hh"
#include "utils.hh"
#include "vector.hh"

#include <memory>

namespace Kakoune
{

struct disconnected : runtime_error
{
    disconnected(String what, bool graceful = false)
      : runtime_error{std::move(what)}, m_graceful{graceful} {}

    const bool m_graceful;
};

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
    RemoteClient(StringView session, std::unique_ptr<UserInterface>&& ui,
                 const EnvVarMap& env_vars, StringView init_command,
                 Optional<BufferCoord> init_coord);

private:
    std::unique_ptr<UserInterface> m_ui;
    std::unique_ptr<FDWatcher>     m_socket_watcher;
    RemoteBuffer                   m_send_buffer;
};

void send_command(StringView session, StringView command);

struct Server : public Singleton<Server>
{
    Server(pid_t pid, String session_name);
    ~Server();
    const pid_t &pid() const { return m_pid; }
    const String& session() const { return m_session; }

    bool rename_session(StringView name);
    void close_session(bool do_unlink = true);

private:
    class Accepter;
    void remove_accepter(Accepter* accepter);

    pid_t m_pid;
    String m_session;
    std::unique_ptr<FDWatcher> m_listener;
    Vector<std::unique_ptr<Accepter>, MemoryDomain::Remote> m_accepters;
};

bool check_session(StringView session);

}

#endif // remote_hh_INCLUDED
