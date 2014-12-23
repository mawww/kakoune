#ifndef scope_hh_INCLUDED
#define scope_hh_INCLUDED

#include "alias_registry.hh"
#include "hook_manager.hh"
#include "keymap_manager.hh"
#include "option_manager.hh"
#include "utils.hh"

namespace Kakoune
{

class Scope
{
public:
    Scope(Scope& parent)
          : m_options(parent.options()),
            m_hooks(parent.hooks()),
            m_keymaps(parent.keymaps()),
            m_aliases(parent.aliases()) {}

    OptionManager&       options()       { return m_options; }
    const OptionManager& options() const { return m_options; }
    HookManager&         hooks()         { return m_hooks; }
    const HookManager&   hooks()   const { return m_hooks; }
    KeymapManager&       keymaps()       { return m_keymaps; }
    const KeymapManager& keymaps() const { return m_keymaps; }
    AliasRegistry&       aliases()       { return m_aliases; }
    const AliasRegistry& aliases() const { return m_aliases; }

private:
    friend class GlobalScope;
    Scope() = default;

    OptionManager m_options;
    HookManager   m_hooks;
    KeymapManager m_keymaps;
    AliasRegistry m_aliases;
};

class GlobalScope : public Scope, public Singleton<GlobalScope>
{
    public:
        GlobalScope() : m_option_registry(m_options) {}

        OptionsRegistry& option_registry() { return m_option_registry; }
        const OptionsRegistry& option_registry() const { return m_option_registry; }
    private:
        OptionsRegistry m_option_registry;
};

}

#endif // scope_hh_INCLUDED
