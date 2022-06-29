#include "scope.hh"
#include "context.hh"

namespace Kakoune
{

void Scope::add_linked(SharedScope& scope, Context& context) {
    if (contains(m_linked, &scope))
        throw runtime_error("scope already linked");
        
    m_linked.emplace_back(&scope);
    
    m_hooks.add_linked(scope.hooks());
    m_keymaps.add_linked(scope.keymaps());
    
    m_hooks.run_hook(Hook::ScopeLinked, scope.get_name(), context);
}

void Scope::remove_linked(SharedScope& scope, Context& context) {
    if (!contains(m_linked, &scope))
        throw runtime_error("scope not linked");
        
    unordered_erase(m_linked, SafePtr<SharedScope>(&scope));
    
    m_hooks.remove_linked(scope.hooks());
    m_keymaps.remove_linked(scope.keymaps());
    
    m_hooks.run_hook(Hook::ScopeUnlinked, scope.get_name(), context);
}

Completions Scope::complete_linked(StringView id, ByteCount cursor_pos) {
    auto candidates = complete(
        id, cursor_pos,
        m_linked | transform([](auto& scope) { return scope->get_name(); })
                 | gather<Vector<String>>());
    return { 0, 0, std::move(candidates) };
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

void SharedScopeManager::register_scope(String name)
{
    auto it = m_scopes.find(name);
    if (it != m_scopes.end())
    {
        throw runtime_error(format("duplicate id: '{}'", name));
    }

    auto scope = std::make_unique<SharedScope>(name);
    m_scopes.insert({std::move(name), std::move(scope)});
}

void SharedScopeManager::remove_scope(StringView id)
{
    m_scopes.remove(id);
}

SharedScope* SharedScopeManager::get_scope_ifp(StringView id)
{
    auto it = m_scopes.find(id);
    if (it == m_scopes.end()) {
      return nullptr;
    }
    return it->value.get();
}

SharedScope& SharedScopeManager::get_scope(StringView id)
{
    if (auto s = get_scope_ifp(id)) 
      return *s;
    throw scope_not_found(format("no such scope: {}", id));
}

Completions SharedScopeManager::complete_scope(StringView id, ByteCount cursor_pos) const
{
    auto candidates = complete(
        id, cursor_pos,
        m_scopes | transform([](auto& hl) { return hl.key; })
                 | gather<Vector<String>>());
    return { 0, 0, std::move(candidates) };
}

}
