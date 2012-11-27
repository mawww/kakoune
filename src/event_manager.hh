#ifndef event_manager_hh_INCLUDED
#define event_manager_hh_INCLUDED

#include <poll.h>
#include <unordered_map>

#include "utils.hh"

namespace Kakoune
{

using EventHandler = std::function<void (int fd)>;

// The EventManager provides an interface to file descriptor
// based event handling.
//
// The program main loop should call handle_next_events()
// until it's time to quit.
class EventManager : public Singleton<EventManager>
{
public:
    // Watch the given file descriptor, when data becomes
    // ready, handler will be called with fd as parameter.
    // It is an error to register multiple handlers on the
    // same file descriptor.
    void watch(int fd, EventHandler handler);

    // stop watching fd
    void unwatch(int fd);

    void handle_next_events();

    // force the handler associated with fd to be executed
    // on next handle_next_events call.
    void force_signal(int fd);

private:
    std::vector<pollfd>                        m_events;
    std::vector<std::unique_ptr<EventHandler>> m_handlers;
    std::vector<std::unique_ptr<EventHandler>> m_handlers_trash;
    std::vector<int>                           m_forced;
};

}

#endif // event_manager_hh_INCLUDED

