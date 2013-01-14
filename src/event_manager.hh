#ifndef event_manager_hh_INCLUDED
#define event_manager_hh_INCLUDED

#include "utils.hh"

#include <chrono>

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

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

class Timer
{
public:
    using Callback = std::function<void (Timer& timer)>;

    Timer(TimePoint date, Callback callback);
    ~Timer();

    TimePoint next_date() const { return m_date; }
    void      set_next_date(TimePoint date) { m_date = date; }
    void run();

private:
    TimePoint m_date;
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

    void handle_next_events();

    // force the watchers associated with fd to be executed
    // on next handle_next_events call.
    void force_signal(int fd);

private:
    friend class FDWatcher;
    friend class Timer;
    Set<FDWatcher*>  m_fd_watchers;
    Set<Timer*>      m_timers;
    std::vector<int> m_forced_fd;

    TimePoint        m_last;
};

}

#endif // event_manager_hh_INCLUDED

