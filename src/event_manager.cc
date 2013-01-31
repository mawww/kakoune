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

Timer::Timer(TimePoint date, Callback callback)
    : m_date{date}, m_callback{std::move(callback)}
{
    EventManager::instance().m_timers.insert(this);
}

Timer::~Timer()
{
    EventManager::instance().m_timers.erase(this);
}

void Timer::run()
{
    m_date = TimePoint::max();
    m_callback(*this);
}

EventManager::EventManager()
{
    m_forced_fd.reserve(4);
}

EventManager::~EventManager()
{
    assert(m_fd_watchers.empty());
    assert(m_timers.empty());
}

void EventManager::handle_next_events()
{
    const int timeout_ms = 100;
    std::vector<pollfd> events;
    events.reserve(m_fd_watchers.size());
    for (auto& watcher : m_fd_watchers)
        events.emplace_back(pollfd{ watcher->fd(), POLLIN | POLLPRI, 0 });
    std::vector<int> forced = m_forced_fd;
    m_forced_fd.clear();
    poll(events.data(), events.size(), timeout_ms);
    for (size_t i = 0; i < events.size(); ++i)
    {
        auto& event = events[i];
        const int fd = event.fd;
        if (event.revents or contains(forced, fd))
        {
            auto it = find_if(m_fd_watchers,
                              [fd](FDWatcher* w) { return w->fd() == fd; });
            if (it != m_fd_watchers.end())
                (*it)->run();
        }
    }

    TimePoint now = Clock::now();
    for (auto& timer : m_timers)
    {
        if (timer->next_date() <= now)
            timer->run();
    }
}

void EventManager::force_signal(int fd)
{
    m_forced_fd.push_back(fd);

}

}
