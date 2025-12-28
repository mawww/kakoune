#include "scope.hh"

#include "alias_registry.hh"
#include "face_registry.hh"
#include "highlighter_group.hh"
#include "hook_manager.hh"
#include "keymap_manager.hh"
#include "option_manager.hh"
#include "context.hh"

namespace Kakoune
{

struct Scope::Data
{
    OptionManager options;
    HookManager   hooks;
    KeymapManager keymaps;
    AliasRegistry aliases;
    FaceRegistry  faces;
    Highlighters  highlighters;
};

Scope::Scope() : m_data(make_unique_ptr<Data>()) {}

Scope::Scope(Scope& parent)
    : m_data(make_unique_ptr<Data>(parent.options(),
                                   parent.hooks(),
                                   parent.keymaps(),
                                   parent.aliases(),
                                   parent.faces(),
                                   parent.highlighters()))
{}

Scope::~Scope() = default;

OptionManager&       Scope::options()            { return m_data->options; }
const OptionManager& Scope::options()      const { return m_data->options; }
HookManager&         Scope::hooks()              { return m_data->hooks; }
const HookManager&   Scope::hooks()        const { return m_data->hooks; }
KeymapManager&       Scope::keymaps()            { return m_data->keymaps; }
const KeymapManager& Scope::keymaps()      const { return m_data->keymaps; }
AliasRegistry&       Scope::aliases()            { return m_data->aliases; }
const AliasRegistry& Scope::aliases()      const { return m_data->aliases; }
FaceRegistry&        Scope::faces()              { return m_data->faces; }
const FaceRegistry&  Scope::faces()        const { return m_data->faces; }
Highlighters&        Scope::highlighters()       { return m_data->highlighters; }
const Highlighters&  Scope::highlighters() const { return m_data->highlighters; }

void Scope::reparent(Scope& parent)
{
    m_data->options.reparent(parent.options());
    m_data->hooks.reparent(parent.hooks());
    m_data->keymaps.reparent(parent.keymaps());
    m_data->aliases.reparent(parent.aliases());
    m_data->faces.reparent(parent.faces());
    m_data->highlighters.reparent(parent.highlighters());
}

struct GlobalScope::GlobalData final : public OptionWatcher
{
    GlobalData(GlobalScope& parent)
    : m_parent(parent),
      m_option_registry(parent.options())
    {
        m_parent.options().register_watcher(*this);
    }

    ~GlobalData()
    {
        m_parent.options().unregister_watcher(*this);
    }

    void on_option_changed(const Option& option) override
    {
        Context empty_context{Context::EmptyContextFlag{}};
        m_parent.hooks().run_hook(Hook::GlobalSetOption,
                                  format("{}={}", option.name(), option.get_desc_string()),
                                  empty_context);
    }

    Scope& m_parent;
    OptionsRegistry m_option_registry;
};

GlobalScope::GlobalScope()
    : m_global_data(make_unique_ptr<GlobalData>(*this))
{
}

GlobalScope::~GlobalScope() = default;

OptionsRegistry& GlobalScope::option_registry() { return m_global_data->m_option_registry; }
const OptionsRegistry& GlobalScope::option_registry() const { return m_global_data->m_option_registry; }

}
