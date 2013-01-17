#ifndef hook_manager_hh_INCLUDED
#define hook_manager_hh_INCLUDED

#include "utils.hh"

#include <unordered_map>

namespace Kakoune
{

class Context;
typedef std::function<void (const String&, Context&)> HookFunc;

class HookManager
{
public:
    HookManager(HookManager& parent) : m_parent(&parent) {}

    void add_hook(const String& hook_name, HookFunc hook);
    void run_hook(const String& hook_name, const String& param,
                  Context& context) const;

private:
    HookManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root hook manager
    friend class GlobalHooks;

    HookManager* m_parent;
    std::unordered_map<String, std::vector<HookFunc>> m_hook;
};

class GlobalHooks : public HookManager,
                    public Singleton<GlobalHooks>
{
};

}

#endif // hook_manager_hh_INCLUDED

