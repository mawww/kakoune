#ifndef event_manager_hh_INCLUDED
#define event_manager_hh_INCLUDED

#include "utils.hh"
#include "flags.hh"

#include <chrono>
#include <vector>
#include <functional>

#include <sys/select.h>

namespace Kakoune
{

enum class EventMode
{
    Normal,
    Urgent,
    Pending
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
    bool closed() const { return m_fd == -1; }
private:

    int       m_fd;
    Callback  m_callback;
};

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

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

    void handle_next_events(EventMode mode);

    // force the watchers associated with fd to be executed
    // on next handle_next_events call.
    void force_signal(int fd);

private:
    friend class FDWatcher;
    friend class Timer;
    std::vector<FDWatcher*>  m_fd_watchers;
    std::vector<Timer*>      m_timers;
    fd_set m_forced_fd;

    TimePoint        m_last;
};

}

#endif // event_manager_hh_INCLUDED

