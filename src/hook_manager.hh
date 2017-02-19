#ifndef hook_manager_hh_INCLUDED
#define hook_manager_hh_INCLUDED

#include "id_map.hh"
#include "completion.hh"
#include "safe_ptr.hh"

namespace Kakoune
{

class Context;
using HookFunc = std::function<void (StringView, Context&)>;

class HookManager : public SafeCountable
{
public:
    HookManager(HookManager& parent) : m_parent(&parent) {}

    void add_hook(StringView hook_name, String group, HookFunc hook);
    void remove_hooks(StringView group);
    CandidateList complete_hook_group(StringView prefix, ByteCount pos_in_token);
    void run_hook(StringView hook_name, StringView param,
                  Context& context) const;

private:
    HookManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root hook manager
    friend class Scope;

    SafePtr<HookManager> m_parent;
    IdMap<IdMap<HookFunc, MemoryDomain::Hooks>, MemoryDomain::Hooks> m_hooks;
    mutable Vector<std::pair<StringView, StringView>, MemoryDomain::Hooks> m_running_hooks;
};

}

#endif // hook_manager_hh_INCLUDED
