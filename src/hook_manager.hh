#ifndef hook_manager_hh_INCLUDED
#define hook_manager_hh_INCLUDED

#include "id_map.hh"
#include "utils.hh"

#include <unordered_map>

namespace Kakoune
{

class Context;
using HookFunc = std::function<void (const String&, Context&)>;

class HookManager
{
public:
    HookManager(HookManager& parent) : m_parent(&parent) {}

    void add_hook(const String& hook_name, String group, HookFunc hook);
    void remove_hooks(StringView group);
    CandidateList complete_hook_group(StringView prefix, ByteCount pos_in_token);
    void run_hook(const String& hook_name, const String& param,
                  Context& context) const;

private:
    HookManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root hook manager
    friend class GlobalHooks;

    HookManager* m_parent;
    std::unordered_map<String, id_map<HookFunc>> m_hook;
};

class GlobalHooks : public HookManager,
                    public Singleton<GlobalHooks>
{
};

}

#endif // hook_manager_hh_INCLUDED

