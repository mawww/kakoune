#include "scope.hh"
#include "context.hh"

namespace Kakoune
{

void Scope::reparent(Scope& parent)
{
    m_options.reparent(parent.m_options);
    m_hooks.reparent(parent.m_hooks);
    m_keymaps.reparent(parent.m_keymaps);
    m_aliases.reparent(parent.m_aliases);
    m_faces.reparent(parent.m_faces);
    m_highlighters.reparent(parent.m_highlighters);
}

GlobalScope::GlobalScope()
    : m_option_registry(m_options)
{
    options().register_watcher(*this);
}

GlobalScope::~GlobalScope()
{
    options().unregister_watcher(*this);
}

void GlobalScope::on_option_changed(const Option& option)
{
    Context empty_context{Context::EmptyContextFlag{}};
    hooks().run_hook(Hook::GlobalSetOption,
                     format("{}={}", option.name(), option.get_desc_string()),
                     empty_context);
}

}
