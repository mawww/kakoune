#ifndef event_manager_hh_INCLUDED
#define event_manager_hh_INCLUDED

#include "clock.hh"
#include "utils.hh"
#include "flags.hh"
#include "vector.hh"

#include <functional>

#include <sys/select.h>
#include <signal.h>

namespace Kakoune
{

enum class EventMode
{
    Normal,
    Urgent,
};

class FDWatcher
{
public:
    using Callback = std::function<void (FDWatcher& watcher, EventMode mode)>;
    FDWatcher(int fd, Callback callback);
    FDWatcher(const FDWatcher&) = delete;
    FDWatcher& operator=(const FDWatcher&) = delete;
    ~FDWatcher();

    int fd() const { return m_fd; }
    void run(EventMode mode);

    void close_fd();
    void disable() { m_fd = -1; }
private:

    int       m_fd;
    Callback  m_callback;
};

class Timer
{
public:
    using Callback = std::function<void (Timer& timer)>;

    Timer(TimePoint date, Callback callback,
          EventMode mode = EventMode::Normal);
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    ~Timer();

    TimePoint next_date() const { return m_date; }
    void      set_next_date(TimePoint date) { m_date = date; }
    void run(EventMode mode);

private:
    TimePoint m_date;
    EventMode m_mode;
    Callback  m_callback;
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

    void handle_next_events(EventMode mode, sigset_t* sigmask = nullptr);

    // force the watchers associated with fd to be executed
    // on next handle_next_events call.
    void force_signal(int fd);

private:
    friend class FDWatcher;
    friend class Timer;
    Vector<FDWatcher*, MemoryDomain::Events> m_fd_watchers;
    Vector<Timer*, MemoryDomain::Events>     m_timers;
    fd_set m_forced_fd;

    TimePoint m_last;
};

using SignalHandler = void(*)(int);

SignalHandler set_signal_handler(int signum, SignalHandler handler);

}

#endif // event_manager_hh_INCLUDED
