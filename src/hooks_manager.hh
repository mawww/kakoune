#ifndef hooks_manager_hh_INCLUDED
#define hooks_manager_hh_INCLUDED

#include "utils.hh"

#include <unordered_map>

namespace Kakoune
{

class Context;
typedef std::function<void (const std::string&, const Context&)> HookFunc;

class HooksManager
{
public:
    void add_hook(const std::string& hook_name, HookFunc hook);
    void run_hook(const std::string& hook_name, const std::string& param,
                  const Context& context) const;

private:
    std::unordered_map<std::string, std::vector<HookFunc>> m_hooks;
};

class GlobalHooksManager : public HooksManager,
                           public Singleton<GlobalHooksManager>
{
};

}

#endif // hooks_manager_hh_INCLUDED

