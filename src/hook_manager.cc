#include "hook_manager.hh"

#include "buffer_utils.hh"
#include "clock.hh"
#include "command_manager.hh"
#include "context.hh"
#include "display_buffer.hh"
#include "face_registry.hh"
#include "option.hh"
#include "ranges.hh"
#include "regex.hh"

namespace Kakoune
{

struct HookManager::Hook
{
    String group;
    Regex filter;
    String commands;
};

HookManager::HookManager() : m_parent(nullptr) {}
HookManager::HookManager(HookManager& parent) : SafeCountable{}, m_parent(&parent) {}
HookManager::~HookManager() = default;

void HookManager::add_hook(StringView hook_name, String group, Regex filter, String commands)
{
    auto& hooks = m_hooks[hook_name];
    hooks.emplace_back(new Hook{std::move(group), std::move(filter), std::move(commands)});
}

void HookManager::remove_hooks(StringView group)
{
    if (group.empty())
        throw runtime_error("invalid id");
    for (auto& list : m_hooks)
    {
        auto it = std::remove_if(list.value.begin(), list.value.end(),
                                 [&](const std::unique_ptr<Hook>& h)
                                 { return h->group == group; });
        if (not m_running_hooks.empty()) // we are running some hooks, defer deletion
            m_hooks_trash.insert(m_hooks_trash.end(), std::make_move_iterator(it),
                                 std::make_move_iterator(list.value.end()));
        list.value.erase(it, list.value.end());
    }
}

CandidateList HookManager::complete_hook_group(StringView prefix, ByteCount pos_in_token)
{
    CandidateList res;
    for (auto& list : m_hooks)
    {
        auto container = list.value | transform([](const std::unique_ptr<Hook>& h) -> const String& { return h->group; });
        for (auto& c : complete(prefix, pos_in_token, container))
        {
            if (!contains(res, c))
                res.push_back(c);
        }
    }
    return res;
}

void HookManager::run_hook(StringView hook_name, StringView param, Context& context) const
{
    if (context.hooks_disabled())
        return;

    if (m_parent)
        m_parent->run_hook(hook_name, param, context);

    auto hook_list = m_hooks.find(hook_name);
    if (hook_list == m_hooks.end())
        return;

    if (contains(m_running_hooks, std::make_pair(hook_name, param)))
    {
        auto error = format("recursive call of hook {}/{}, not executing", hook_name, param);
        write_to_debug_buffer(error);
        return;
    }

    m_running_hooks.emplace_back(hook_name, param);
    auto pop_running_hook = on_scope_end([this]{
        m_running_hooks.pop_back();
        if (m_running_hooks.empty())
            m_hooks_trash.clear();
    });

    const DebugFlags debug_flags = context.options()["debug"].get<DebugFlags>();
    const bool profile = debug_flags & DebugFlags::Profile;
    auto start_time = profile ? Clock::now() : TimePoint{};

    auto& disabled_hooks = context.options()["disabled_hooks"].get<Regex>();

    struct ToRun { Hook* hook; MatchResults<const char*> captures; };
    Vector<ToRun> hooks_to_run; // The m_hooks_trash vector ensure hooks wont die during this method
    for (auto& hook : hook_list->value)
    {
        MatchResults<const char*> captures;
        if ((hook->group.empty() or disabled_hooks.empty() or
             not regex_match(hook->group.begin(), hook->group.end(), disabled_hooks))
            and regex_match(param.begin(), param.end(), captures, hook->filter))
            hooks_to_run.push_back({ hook.get(), std::move(captures) });
    }

    bool hook_error = false;
    for (auto& to_run : hooks_to_run)
    {
        try
        {
            if (debug_flags & DebugFlags::Hooks)
                write_to_debug_buffer(format("hook {}({})/{}", hook_name, param, to_run.hook->group));

            ScopedSetBool disable_history{context.history_disabled()};

            EnvVarMap env_vars{ {"hook_param", param.str()} };
            for (size_t i = 0; i < to_run.captures.size(); ++i)
                env_vars.insert({format("hook_param_capture_{}", i),
                                 {to_run.captures[i].first, to_run.captures[i].second}});

            CommandManager::instance().execute(to_run.hook->commands, context,
                                               { {}, std::move(env_vars) });
        }
        catch (runtime_error& err)
        {
            hook_error = true;
            write_to_debug_buffer(format("error running hook {}({})/{}: {}",
                               hook_name, param, to_run.hook->group, err.what()));
        }
    }

    if (hook_error)
        context.print_status({
            format("Error running hooks for '{}' '{}', see *debug* buffer",
                   hook_name, param), get_face("Error") });

    if (profile)
    {
        auto end_time = Clock::now();
        auto full = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        write_to_debug_buffer(format("hook '{}({})' took {} us", hook_name, param, (size_t)full.count()));
    }
}

}
