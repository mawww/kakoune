#include "event_manager.hh"

#include "containers.hh"

#include <unistd.h>

namespace Kakoune
{

FDWatcher::FDWatcher(int fd, Callback callback)
    : m_fd{fd}, m_callback{std::move(callback)}
{
    EventManager::instance().m_fd_watchers.push_back(this);
}

FDWatcher::~FDWatcher()
{
    unordered_erase(EventManager::instance().m_fd_watchers, this);
}

void FDWatcher::run(EventMode mode)
{
    m_callback(*this, mode);
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
    fd_set rfds;
    FD_ZERO(&rfds);
    for (auto& watcher : m_fd_watchers)
    {
        const int fd = watcher->fd();
        if (fd != -1)
        {
            max_fd = std::max(fd, max_fd);
            FD_SET(fd, &rfds);
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
    int res = pselect(max_fd + 1, &rfds, nullptr, nullptr,
                      with_timeout ? &ts : nullptr, sigmask);

    // copy forced fds *after* select, so that signal handlers can write to
    // m_forced_fd, interupt select, and directly be serviced.
    fd_set forced = m_forced_fd;
    FD_ZERO(&m_forced_fd);

    for (int fd = 0; fd < max_fd + 1; ++fd)
    {
        if ((res > 0 and FD_ISSET(fd, &rfds)) or FD_ISSET(fd, &forced))
        {
            auto it = find_if(m_fd_watchers,
                              [fd](const FDWatcher* w){return w->fd() == fd; });
            if (it != m_fd_watchers.end())
                (*it)->run(mode);
        }
    }

    TimePoint now = Clock::now();
    for (auto& timer : m_timers)
    {
        if (timer->next_date() <= now)
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
