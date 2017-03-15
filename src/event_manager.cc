#include "event_manager.hh"

#include "containers.hh"
#include "flags.hh"

#include <unistd.h>

namespace Kakoune
{

FDWatcher::FDWatcher(int fd, FdEvents events, Callback callback)
    : m_fd{fd}, m_events{events}, m_callback{std::move(callback)}
{
    EventManager::instance().m_fd_watchers.push_back(this);
}

FDWatcher::~FDWatcher()
{
    unordered_erase(EventManager::instance().m_fd_watchers, this);
}

void FDWatcher::run(FdEvents events, EventMode mode)
{
    m_callback(*this, events, mode);
}

void FDWatcher::close_fd()
{
    if (m_fd != -1)
    {
        close(m_fd);
        m_fd = -1;
    }
}

Timer::Timer(TimePoint date, Callback callback, EventMode mode)
    : m_date{date}, m_callback{std::move(callback)}, m_mode(mode)
{
    if (m_callback and EventManager::has_instance())
        EventManager::instance().m_timers.push_back(this);
}

Timer::~Timer()
{
    if (m_callback and EventManager::has_instance())
        unordered_erase(EventManager::instance().m_timers, this);
}

void Timer::run(EventMode mode)
{
    kak_assert(m_callback);
    if (mode == m_mode)
    {
        m_date = TimePoint::max();
        m_callback(*this);
    }
    else // try again a little later
        m_date = Clock::now() + std::chrono::milliseconds{10};
}

EventManager::EventManager()
{
    FD_ZERO(&m_forced_fd);
}

EventManager::~EventManager()
{
    kak_assert(m_fd_watchers.empty());
    kak_assert(m_timers.empty());
}

void EventManager::handle_next_events(EventMode mode, sigset_t* sigmask)
{
    int max_fd = 0;
    fd_set rfds, wfds, efds;
    FD_ZERO(&rfds); FD_ZERO(&wfds); FD_ZERO(&efds);
    for (auto& watcher : m_fd_watchers)
    {
        const int fd = watcher->fd();
        if (fd != -1)
        {
            max_fd = std::max(fd, max_fd);
            auto events = watcher->events();
            if (events & FdEvents::Read)
                FD_SET(fd, &rfds);
            if (events & FdEvents::Write)
                FD_SET(fd, &wfds);
            if (events & FdEvents::Except)
                FD_SET(fd, &efds);
        }
    }

    bool with_timeout = false;
    timespec ts{};
    if (not m_timers.empty())
    {
        auto next_date = (*std::min_element(
            m_timers.begin(), m_timers.end(), [](Timer* lhs, Timer* rhs) {
                return lhs->next_date() < rhs->next_date();
        }))->next_date();

        if (next_date != TimePoint::max())
        {
            with_timeout = true;
            using namespace std::chrono; using ns = std::chrono::nanoseconds;
            auto nsecs = std::max(ns(0), duration_cast<ns>(next_date - Clock::now()));
            auto secs = duration_cast<seconds>(nsecs);
            ts = timespec{ (time_t)secs.count(), (long)(nsecs - secs).count() };
        }
    }
    int res = pselect(max_fd + 1, &rfds, &wfds, &efds,
                      with_timeout ? &ts : nullptr, sigmask);

    // copy forced fds *after* select, so that signal handlers can write to
    // m_forced_fd, interupt select, and directly be serviced.
    fd_set forced = m_forced_fd;
    FD_ZERO(&m_forced_fd);

    for (int fd = 0; fd < max_fd + 1; ++fd)
    {
        auto events =  FD_ISSET(fd, &forced) ? FdEvents::Read : FdEvents::None;
        if (res > 0)
            events |= (FD_ISSET(fd, &rfds) ? FdEvents::Read : FdEvents::None) |
                      (FD_ISSET(fd, &wfds) ? FdEvents::Write : FdEvents::None) |
                      (FD_ISSET(fd, &efds) ? FdEvents::Except : FdEvents::None);

        if (events != FdEvents::None)
        {
            auto it = find_if(m_fd_watchers,
                              [fd](const FDWatcher* w){return w->fd() == fd; });
            if (it != m_fd_watchers.end())
                (*it)->run(events, mode);
        }
    }

    TimePoint now = Clock::now();
    auto timers = m_timers; // copy timers in case m_timers gets mutated
    for (auto& timer : timers)
    {
        if (contains(m_timers, timer) and timer->next_date() <= now)
            timer->run(mode);
    }
}

void EventManager::force_signal(int fd)
{
    FD_SET(fd, &m_forced_fd);
}

SignalHandler set_signal_handler(int signum, SignalHandler handler)
{
    struct sigaction new_action, old_action;

    sigemptyset(&new_action.sa_mask);
    new_action.sa_handler = handler;
    new_action.sa_flags = SA_RESTART;
    sigaction(signum, &new_action, &old_action);
    return old_action.sa_handler;
}
}
