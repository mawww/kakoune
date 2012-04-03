#ifndef hook_manager_hh_INCLUDED
#define hook_manager_hh_INCLUDED

#include "utils.hh"

#include <unordered_map>

namespace Kakoune
{

class Context;
typedef std::function<void (const std::string&, const Context&)> HookFunc;

class HookManager
{
public:
    void add_hook(const std::string& hook_name, HookFunc hook);
    void run_hook(const std::string& hook_name, const std::string& param,
                  const Context& context) const;

private:
    std::unordered_map<std::string, std::vector<HookFunc>> m_hook;
};

class GlobalHookManager : public HookManager,
                          public Singleton<GlobalHookManager>
{
};

}

#endif // hook_manager_hh_INCLUDED

