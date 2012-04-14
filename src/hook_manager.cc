#include "hook_manager.hh"

namespace Kakoune
{

void HookManager::add_hook(const String& hook_name, HookFunc hook)
{
    m_hook[hook_name].push_back(hook);
}

void HookManager::run_hook(const String& hook_name,
                           const String& param,
                           const Context& context) const
{
    auto hook_list_it = m_hook.find(hook_name);
    if (hook_list_it == m_hook.end())
        return;

    for (auto& hook : hook_list_it->second)
    {
        try
        {
            hook(param, context);
        }
        catch (runtime_error&) {}
    }
}

}
