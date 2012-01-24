#include "hooks_manager.hh"

namespace Kakoune
{

void HooksManager::add_hook(const std::string& hook_name, HookFunc hook)
{
    m_hooks[hook_name].push_back(hook);
}

void HooksManager::run_hook(const std::string& hook_name,
                            const std::string& param,
                            const Context& context) const
{
    auto hook_list_it = m_hooks.find(hook_name);
    if (hook_list_it == m_hooks.end())
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
