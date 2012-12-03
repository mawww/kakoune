#include "event_manager.hh"

#include <algorithm>

namespace Kakoune
{

EventManager::EventManager()
{
    m_forced.reserve(4);
}

void EventManager::watch(int fd, EventHandler handler)
{
    auto event = std::find_if(m_events.begin(), m_events.end(),
                              [&](const pollfd& pfd) { return pfd.fd == fd; });
    if (event != m_events.end())
        throw runtime_error("fd already watched");

    m_events.emplace_back(pollfd{ fd, POLLIN | POLLPRI, 0 });
    m_handlers.emplace_back(new EventHandler(std::move(handler)));
}

void EventManager::unwatch(int fd)
{
    auto event = std::find_if(m_events.begin(), m_events.end(),
                              [&](const pollfd& pfd) { return pfd.fd == fd; });
    assert(event != m_events.end());
    auto handler = m_handlers.begin() + (event - m_events.begin());

    // keep handler in m_handlers_trash so that it does not die now,
    // but at the end of handle_next_events. We do this as handler might
    // be our caller.
    m_handlers_trash.emplace_back(std::move(*handler));
    m_handlers.erase(handler);
    m_events.erase(event);
}

void EventManager::handle_next_events()
{
    const int timeout_ms = 100;
    poll(m_events.data(), m_events.size(), timeout_ms);
    std::vector<int> forced = m_forced;
    m_forced.clear();
    for (size_t i = 0; i < m_events.size(); ++i)
    {
        auto& event = m_events[i];
        const int fd = event.fd;
        if (event.revents or contains(forced, fd))
            (*m_handlers[i])(fd);
    }
    m_handlers_trash.clear();
}

void EventManager::force_signal(int fd)
{
     m_forced.push_back(fd);
}

}
