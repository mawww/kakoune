#ifndef hook_manager_hh_INCLUDED
#define hook_manager_hh_INCLUDED

#include "id_map.hh"
#include "completion.hh"
#include "unordered_map.hh"

namespace Kakoune
{

class Context;
using HookFunc = std::function<void (StringView, Context&)>;

class HookManager
{
public:
    HookManager(HookManager& parent) : m_parent(&parent) {}

    void add_hook(const String& hook_name, String group, HookFunc hook);
    void remove_hooks(StringView group);
    CandidateList complete_hook_group(StringView prefix, ByteCount pos_in_token);
    void run_hook(const String& hook_name, StringView param,
                  Context& context) const;

private:
    HookManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root hook manager
    friend class Scope;

    HookManager* m_parent;
    UnorderedMap<String, IdMap<HookFunc, MemoryDomain::Hooks>, MemoryDomain::Hooks> m_hook;
};

}

#endif // hook_manager_hh_INCLUDED
