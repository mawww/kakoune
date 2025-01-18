#ifndef profile_hh_INCLUDED
#define profile_hh_INCLUDED

#include "clock.hh"
#include "context.hh"
#include "debug.hh"
#include "option_manager.hh"

namespace Kakoune
{

template<typename Callback>
class ProfileScope
{
public:
    ProfileScope(const DebugFlags debug_flags, Callback&& callback, bool active = true)
      : m_active{active and debug_flags & DebugFlags::Profile},
        m_start_time(m_active ? Clock::now() : Clock::time_point{}),
        m_callback{std::move(callback)}
    {}

    ProfileScope(const Context& context, Callback&& callback, bool active = true)
      : ProfileScope{context.options()["debug"].get<DebugFlags>(), std::move(callback), active}
    {}

    ~ProfileScope()
    {
        if (m_active)
            m_callback(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - m_start_time));
    }

private:
    bool m_active;
    Clock::time_point m_start_time;
    Callback m_callback;
};

}

#endif // profile_hh_INCLUDED
