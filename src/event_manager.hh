#ifndef event_manager_hh_INCLUDED
#define event_manager_hh_INCLUDED

#include "utils.hh"

namespace Kakoune
{

class FDWatcher
{
public:
    using Callback = std::function<void (FDWatcher& watcher)>;
    FDWatcher(int fd, Callback callback);
    ~FDWatcher();

    int fd() const { return m_fd; }
    void run() { m_callback(*this); }
private:
    int      m_fd;
    Callback m_callback;
};

// The EventManager provides an interface to file descriptor
// based event handling.
//
// The program main loop should call handle_next_events()
// until it's time to quit.
class EventManager : public Singleton<EventManager>
{
public:
    EventManager();
    ~EventManager();

    void handle_next_events();

    // force the watchers associated with fd to be executed
    // on next handle_next_events call.
    void force_signal(int fd);

private:
    friend class FDWatcher;
    Set<FDWatcher*>  m_fd_watchers;
    std::vector<int> m_forced_fd;
};

}

#endif // event_manager_hh_INCLUDED

