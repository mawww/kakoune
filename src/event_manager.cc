#include "event_manager.hh"

#include <poll.h>

namespace Kakoune
{

FDWatcher::FDWatcher(int fd, Callback callback)
    : m_fd{fd}, m_callback{std::move(callback)}
{
    EventManager::instance().register_fd_watcher(this);
}

FDWatcher::~FDWatcher()
{
    EventManager::instance().unregister_fd_watcher(this);
}

EventManager::EventManager()
{
    m_forced_fd.reserve(4);
}

EventManager::~EventManager()
{
    assert(m_fd_watchers.empty());
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
            auto it = std::find_if(m_fd_watchers.begin(), m_fd_watchers.end(),
                                   [fd](FDWatcher* w) { return w->fd() == fd; });
            if (it != m_fd_watchers.end())
                (*it)->run();
        }
    }
}

void EventManager::force_signal(int fd)
{
    m_forced_fd.push_back(fd);

}
void EventManager::register_fd_watcher(FDWatcher* event)
{
    assert(not contains(m_fd_watchers, event));
    m_fd_watchers.push_back(event);
}

void EventManager::unregister_fd_watcher(FDWatcher* event)
{
    auto it = find(m_fd_watchers, event);
    assert(it != m_fd_watchers.end());
    m_fd_watchers.erase(it);
}

}
