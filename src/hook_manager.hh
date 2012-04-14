#ifndef hook_manager_hh_INCLUDED
#define hook_manager_hh_INCLUDED

#include "utils.hh"

#include <unordered_map>

namespace Kakoune
{

class Context;
typedef std::function<void (const String&, const Context&)> HookFunc;

class HookManager
{
public:
    void add_hook(const String& hook_name, HookFunc hook);
    void run_hook(const String& hook_name, const String& param,
                  const Context& context) const;

private:
    std::unordered_map<String, std::vector<HookFunc>> m_hook;
};

class GlobalHookManager : public HookManager,
                          public Singleton<GlobalHookManager>
{
};

}

#endif // hook_manager_hh_INCLUDED

