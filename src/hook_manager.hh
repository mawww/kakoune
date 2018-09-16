#ifndef hook_manager_hh_INCLUDED
#define hook_manager_hh_INCLUDED

#include "hash_map.hh"
#include "completion.hh"
#include "safe_ptr.hh"

namespace Kakoune
{

class Context;
class Regex;

enum class HookFlags
{
    None       = 0,
    NoDisabled = 1 << 0,
    Once       = 1 << 1
};
constexpr bool with_bit_ops(Meta::Type<HookFlags>) { return true; }

class HookManager : public SafeCountable
{
public:
    HookManager(HookManager& parent);
    ~HookManager();

    void add_hook(StringView hook_name, String group, HookFlags flags,
                  Regex filter, String commands);
    void remove_hooks(const Regex& regex);
    CandidateList complete_hook_group(StringView prefix, ByteCount pos_in_token);
    void run_hook(StringView hook_name, StringView param,
                  Context& context);

private:
    HookManager();
    // the only one allowed to construct a root hook manager
    friend class Scope;

    struct Hook;

    SafePtr<HookManager> m_parent;
    HashMap<String, Vector<std::unique_ptr<Hook>, MemoryDomain::Hooks>, MemoryDomain::Hooks> m_hooks;

    mutable Vector<std::pair<StringView, StringView>, MemoryDomain::Hooks> m_running_hooks;
    mutable Vector<std::unique_ptr<Hook>, MemoryDomain::Hooks> m_hooks_trash;
};

}

#endif // hook_manager_hh_INCLUDED
