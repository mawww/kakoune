#include "hook_manager.hh"

#include "debug.hh"

namespace Kakoune
{

void HookManager::add_hook(const String& hook_name, HookFunc hook)
{
    m_hook[hook_name].push_back(hook);
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
            hook(param, context);
        }
        catch (runtime_error& err)
        {
            write_debug("error running hook " + hook_name + ": " + err.what());
        }
    }
}

}
