#include "hook_manager.hh"

#include "debug.hh"

namespace Kakoune
{

void HookManager::add_hook(const String& hook_name, String id, HookFunc hook)
{
    auto& hooks = m_hook[hook_name];
    hooks.append({std::move(id), std::move(hook)});
}

void HookManager::remove_hooks(const String& id)
{
    if (id.empty())
        throw runtime_error("invalid id");
    for (auto& hooks : m_hook)
        hooks.second.remove_all(id);
}

void HookManager::run_hook(const String& hook_name,
                           const String& param,
                           Context& context) const
{
    if (m_parent)
        m_parent->run_hook(hook_name, param, context);

    auto hook_list_it = m_hook.find(hook_name);
    if (hook_list_it == m_hook.end())
        return;

    for (auto& hook : hook_list_it->second)
    {
        try
        {
            hook.second(param, context);
        }
        catch (runtime_error& err)
        {
            write_debug("error running hook " + hook_name + "/" +
                        hook.first + ": " + err.what());
        }
    }
}

}
