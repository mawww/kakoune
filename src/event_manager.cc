#include "event_manager.hh"

#include <algorithm>

namespace Kakoune
{

EventManager::EventManager() { m_forced.reserve(4); }

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
    for (size_t i = 0; i < m_events.size(); ++i)
    {
        if (m_events[i].fd == fd)
        {
            m_events.erase(m_events.begin() + i);
            m_handlers.erase(fd);
            return;
        }
    }
}

void EventManager::handle_next_events()
{
    const int timeout_ms = 100;
    int res = poll(m_events.data(), m_events.size(), timeout_ms);
    for (size_t i = 0; i < m_events.size(); ++i)
    {
        if ((res > 0 and m_events[i].revents) or
            contains(m_forced, m_events[i].fd))
            m_handlers[m_events[i].fd](m_events[i].fd);
    }
    m_forced.clear();
}

void EventManager::force_signal(int fd)
{
     m_forced.push_back(fd);
}

}
