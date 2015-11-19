#include "hook_manager.hh"

#include "containers.hh"
#include "context.hh"
#include "buffer_utils.hh"
#include "display_buffer.hh"
#include "face_registry.hh"
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
        hooks.value.remove_all(group);
}

CandidateList HookManager::complete_hook_group(StringView prefix, ByteCount pos_in_token)
{
    CandidateList res;
    for (auto& list : m_hook)
    {
        auto container = transformed(list.value, decltype(list.value)::get_id);
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

    const bool trace = context.options()["debug"].get<DebugFlags>() & DebugFlags::Hooks;

    auto& disabled_hooks = context.options()["disabled_hooks"].get<Regex>();
    bool hook_error = false;
    for (auto& hook : hook_list_it->value)
    {
        if (not hook.key.empty() and not disabled_hooks.empty() and
            regex_match(hook.key.begin(), hook.key.end(), disabled_hooks))
            continue;

        try
        {
            if (trace)
                write_to_debug_buffer(format("hook {}/{}", hook_name, hook.key));

            hook.value(param, context);
        }
        catch (runtime_error& err)
        {
            hook_error = true;
            write_to_debug_buffer(format("error running hook {}/{}: {}",
                               hook_name, hook.key, err.what()));
        }
    }

    if (hook_error)
        context.print_status({
            format("Error running hooks for '{}' '{}', see *debug* buffer",
                   hook_name, param), get_face("Error") });
}

}
