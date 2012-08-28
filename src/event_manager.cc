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
    m_handlers.push_back(std::move(handler));
}

void EventManager::unwatch(int fd)
{
    for (size_t i = 0; i < m_events.size(); ++i)
    {
        if (m_events[i].fd == fd)
        {
            m_events.erase(m_events.begin() + i);
            m_handlers.erase(m_handlers.begin() + i);
            return;
        }
    }
}

void EventManager::handle_next_events()
{
    int res = poll(m_events.data(), m_events.size(), -1);
    if (res > 0)
    {
        for (size_t i = 0; i < m_events.size(); ++i)
        {
            if (m_events[i].revents & POLLIN)
                m_handlers[i](m_events[i].fd);
        }
    }
}

}
