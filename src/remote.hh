#ifndef remote_hh_INCLUDED
#define remote_hh_INCLUDED

#include "env_vars.hh"
#include "exception.hh"
#include "utils.hh"
#include "vector.hh"
#include "optional.hh"

#include <memory>

namespace Kakoune
{

struct disconnected : runtime_error
{
    using runtime_error::runtime_error;
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
                 int pid, const EnvVarMap& env_vars, StringView init_command,
                 Optional<BufferCoord> init_coord);

    const Optional<int>& exit_status() const { return m_exit_status; }
private:
    std::unique_ptr<UserInterface> m_ui;
    std::unique_ptr<FDWatcher>     m_socket_watcher;
    RemoteBuffer                   m_send_buffer;
    Optional<int>                  m_exit_status;
};

void send_command(StringView session, StringView command);
String get_user_name(int uid);

struct Server : public Singleton<Server>
{
    Server(String session_name);
    ~Server();
    const String& session() const { return m_session; }

    bool rename_session(StringView name);
    void close_session(bool do_unlink = true);

    bool negotiating() const { return not m_accepters.empty(); }

private:
    class Accepter;
    void remove_accepter(Accepter* accepter);

    String m_session;
    std::unique_ptr<FDWatcher> m_listener;
    Vector<std::unique_ptr<Accepter>, MemoryDomain::Remote> m_accepters;
};

bool check_session(StringView session);

}

#endif // remote_hh_INCLUDED
