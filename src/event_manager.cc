#include "event_manager.hh"

#include <algorithm>

namespace Kakoune
{

void EventManager::watch(int fd, EventHandler handler)
{
    auto event = std::find_if(m_events.begin(), m_events.end(),
                              [&](const pollfd& pfd) { return pfd.fd == fd; });
    if (event != m_events.end())
        throw runtime_error("fd already watched");

    m_events.push_back(pollfd{ fd, POLLIN | POLLPRI, 0 });
    m_handlers.emplace(fd, std::move(handler));
}

void EventManager::unwatch(int fd)
{
    // do not unwatch now, do that at the end of handle_next_events,
    // so that if unwatch(fd) is called from fd event handler,
    // it is not deleted now.
    m_unwatched.push_back(fd);
}

void EventManager::handle_next_events()
{
    const int timeout_ms = 100;
    poll(m_events.data(), m_events.size(), timeout_ms);
    for (auto& event : m_events)
    {
        const int fd = event.fd;
        if ((event.revents or contains(m_forced, fd)) and not contains(m_unwatched, fd))
            m_handlers[fd](fd);
    }

    // remove unwatched.
    for (auto fd : m_unwatched)
    {
        auto it = std::find_if(m_events.begin(), m_events.end(),
                               [fd](pollfd& p) { return p.fd == fd; });
        m_events.erase(it);
        m_handlers.erase(fd);
    }
    m_unwatched.clear();
    m_forced.clear();
}

void EventManager::force_signal(int fd)
{
     m_forced.push_back(fd);
}

}
