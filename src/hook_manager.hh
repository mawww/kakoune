#ifndef hook_manager_hh_INCLUDED
#define hook_manager_hh_INCLUDED

#include "hash_map.hh"
#include "completion.hh"
#include "safe_ptr.hh"
#include "regex.hh"

namespace Kakoune
{

class Context;

class HookManager : public SafeCountable
{
public:
    HookManager(HookManager& parent) : SafeCountable{}, m_parent(&parent) {}

    void add_hook(StringView hook_name, String group, Regex filter, String commands);
    void remove_hooks(StringView group);
    CandidateList complete_hook_group(StringView prefix, ByteCount pos_in_token);
    void run_hook(StringView hook_name, StringView param,
                  Context& context) const;

private:
    HookManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root hook manager
    friend class Scope;

    struct Hook
    {
        String group;
        Regex filter;
        String commands;
    };

    SafePtr<HookManager> m_parent;
    HashMap<String, Vector<std::unique_ptr<Hook>, MemoryDomain::Hooks>, MemoryDomain::Hooks> m_hooks;

    mutable Vector<std::pair<StringView, StringView>, MemoryDomain::Hooks> m_running_hooks;
    mutable Vector<std::unique_ptr<Hook>, MemoryDomain::Hooks> m_hooks_trash;
};

}

#endif // hook_manager_hh_INCLUDED
