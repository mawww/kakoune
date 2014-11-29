#include "event_manager.hh"

#include <poll.h>

namespace Kakoune
{

FDWatcher::FDWatcher(int fd, Callback callback)
    : m_fd{fd}, m_callback{std::move(callback)}
{
    EventManager::instance().m_fd_watchers.insert(this);
}

FDWatcher::~FDWatcher()
{
    EventManager::instance().m_fd_watchers.erase(this);
}

void FDWatcher::run(EventMode mode)
{
    m_callback(*this, mode);
}

Timer::Timer(TimePoint date, Callback callback, EventMode mode)
    : m_date{date}, m_callback{std::move(callback)}, m_mode(mode)
{
    if (EventManager::has_instance())
        EventManager::instance().m_timers.insert(this);
}

Timer::~Timer()
{
    if (EventManager::has_instance())
        EventManager::instance().m_timers.erase(this);
}

void Timer::run(EventMode mode)
{
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
    m_forced_fd.reserve(4);
}

EventManager::~EventManager()
{
    kak_assert(m_fd_watchers.empty());
    kak_assert(m_timers.empty());
}

void EventManager::handle_next_events(EventMode mode)
{
    std::vector<pollfd> events;
    events.reserve(m_fd_watchers.size());
    for (auto& watcher : m_fd_watchers)
        events.emplace_back(pollfd{ watcher->fd(), POLLIN | POLLPRI, 0 });

    TimePoint next_timer = TimePoint::max();
    for (auto& timer : m_timers)
    {
        if (timer->next_date() <= next_timer)
            next_timer = timer->next_date();
    }
    using namespace std::chrono;
    auto timeout = duration_cast<milliseconds>(next_timer - Clock::now()).count();
    poll(events.data(), events.size(), timeout < INT_MAX ? (int)timeout : INT_MAX);

    // gather forced fds *after* poll, so that signal handlers can write to
    // m_forced_fd, interupt poll, and directly be serviced.
    std::vector<int> forced;
    std::swap(forced, m_forced_fd);

    for (auto& event : events)
    {
        const int fd = event.fd;
        if (event.revents or contains(forced, fd))
        {
            auto it = find_if(m_fd_watchers,
                              [fd](FDWatcher* w) { return w->fd() == fd; });
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
    m_forced_fd.push_back(fd);
}

}
