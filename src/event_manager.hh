#ifndef event_manager_hh_INCLUDED
#define event_manager_hh_INCLUDED

#include <poll.h>

#include "utils.hh"

namespace Kakoune
{

using EventHandler = std::function<void (int fd)>;

class EventManager : public Singleton<EventManager>
{
public:
    void watch(int fd, EventHandler handler);
    void unwatch(int fd);

    void handle_next_events();

private:
    std::vector<pollfd> m_events;
    std::vector<EventHandler> m_handlers;
};

}

#endif // event_manager_hh_INCLUDED

