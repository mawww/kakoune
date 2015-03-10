#include "hook_manager.hh"

#include "containers.hh"
#include "context.hh"
#include "debug.hh"
#include "regex.hh"

namespace Kakoune
{

void HookManager::add_hook(StringView hook_name, String group, HookFunc hook)
{
    auto& hooks = m_hook[hook_name];
    hooks.append({std::move(group), std::move(hook)});
}

void HookManager::remove_hooks(StringView group)
{
    if (group.empty())
        throw runtime_error("invalid id");
    for (auto& hooks : m_hook)
        hooks.second.remove_all(group);
}

CandidateList HookManager::complete_hook_group(StringView prefix, ByteCount pos_in_token)
{
    CandidateList res;
    for (auto& list : m_hook)
    {
        auto container = transformed(list.second, IdMap<HookFunc>::get_id);
        for (auto& c : complete(prefix, pos_in_token, container))
        {
            if (!contains(res, c))
                res.push_back(c);
        }
    }
    return res;
}

void HookManager::run_hook(StringView hook_name,
                           StringView param, Context& context) const
{
    if (m_parent)
        m_parent->run_hook(hook_name, param, context);

    auto hook_list_it = m_hook.find(hook_name);
    if (hook_list_it == m_hook.end())
        return;

    auto& disabled_hooks = context.options()["disabled_hooks"].get<Regex>();
    for (auto& hook : hook_list_it->second)
    {
        if (not hook.first.empty() and not disabled_hooks.empty() and
            regex_match(hook.first.begin(), hook.first.end(), disabled_hooks))
            continue;

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
