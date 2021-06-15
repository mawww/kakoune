#ifndef event_manager_hh_INCLUDED
#define event_manager_hh_INCLUDED

#include "clock.hh"
#include "meta.hh"
#include "utils.hh"
#include "vector.hh"

#include <functional>

#include <sys/select.h>
#include <csignal>

namespace Kakoune
{

enum class EventMode
{
    Normal,
    Urgent,
};

enum class FdEvents
{
    None = 0,
    Read = 1 << 0,
    Write = 1 << 1,
    Except = 1 << 2,
};

constexpr bool with_bit_ops(Meta::Type<FdEvents>) { return true; }

class FDWatcher
{
public:
    using Callback = std::function<void (FDWatcher& watcher, FdEvents events, EventMode mode)>;
    FDWatcher(int fd, FdEvents events, EventMode mode, Callback callback);
    FDWatcher(const FDWatcher&) = delete;
    FDWatcher& operator=(const FDWatcher&) = delete;
    ~FDWatcher();

    int fd() const { return m_fd; }
    FdEvents events() const { return m_events; }
    FdEvents& events() { return m_events; }
    EventMode mode() const { return m_mode; }

    void run(FdEvents events, EventMode mode);

    void reset_fd(int fd) { m_fd = fd; }
    void close_fd();
    void disable() { m_fd = -1; }

private:
    int      m_fd;
    FdEvents m_events;
    EventMode m_mode;
    Callback m_callback;
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
    void set_next_date(TimePoint date) { m_date = date; }
    void disable() { m_date = TimePoint::max(); }
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

    bool handle_next_events(EventMode mode, sigset_t* sigmask = nullptr, bool block = true);

    // force the watchers associated with fd to be executed
    // on next handle_next_events call.
    void force_signal(int fd);

private:
    friend class FDWatcher;
    friend class Timer;
    Vector<FDWatcher*, MemoryDomain::Events> m_fd_watchers;
    Vector<Timer*, MemoryDomain::Events>     m_timers;
    fd_set m_forced_fd;
    bool   m_has_forced_fd = false;

    TimePoint m_last;
};

using SignalHandler = void(*)(int);

SignalHandler set_signal_handler(int signum, SignalHandler handler);

}

#endif // event_manager_hh_INCLUDED
