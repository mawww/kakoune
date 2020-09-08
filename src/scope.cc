#include "scope.hh"
#include "context.hh"
#include "ranges.hh"

namespace Kakoune
{

void Scope::add_shared_scope(Scope& shared_scope)
{
    if (contains(m_shared_scopes, &shared_scope))
        return;

    m_options.add_shared(shared_scope.options());
    m_hooks.add_shared(shared_scope.hooks());
    m_keymaps.add_shared(shared_scope.keymaps());
    m_aliases.add_shared(shared_scope.aliases());
    m_faces.add_shared(shared_scope.faces());
    m_highlighters.add_shared(shared_scope.highlighters());

    m_shared_scopes.push_back(&shared_scope);
}

void Scope::remove_shared_scope(Scope& shared_scope)
{
    auto it = find(m_shared_scopes, &shared_scope);
    if (it == m_shared_scopes.end())
        return;

    m_options.remove_shared(shared_scope.options());
    m_hooks.remove_shared(shared_scope.hooks());
    m_keymaps.remove_shared(shared_scope.keymaps());
    m_aliases.remove_shared(shared_scope.aliases());
    m_faces.remove_shared(shared_scope.faces());
    m_highlighters.remove_shared(shared_scope.highlighters());

    m_shared_scopes.erase(it);
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
