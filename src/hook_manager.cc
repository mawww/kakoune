#include "hook_manager.hh"

#include "debug.hh"
#include "command_manager.hh"
#include "context.hh"
#include "display_buffer.hh"
#include "face_registry.hh"
#include "option.hh"
#include "option_types.hh"
#include "profile.hh"
#include "ranges.hh"
#include "regex.hh"

namespace Kakoune
{

struct HookManager::HookData
{
    String group;
    HookFlags flags;
    Regex filter;
    String commands;

    bool should_run(bool only_always, const Regex& disabled_hooks, StringView param,
                    MatchResults<const char*>& captures) const
    {
        return (not only_always or (flags & HookFlags::Always)) and
                (group.empty() or disabled_hooks.empty() or
                 not regex_match(group.begin(), group.end(), disabled_hooks))
                and regex_match(param.begin(), param.end(), captures, filter);
    }

    void exec(Hook hook, StringView param, Context& context, const MatchResults<const char*>& captures)
    {
        if (context.options()["debug"].get<DebugFlags>() & DebugFlags::Hooks)
            write_to_debug_buffer(format("hook {}({})/{}",
                                  enum_desc(Meta::Type<Hook>{})[to_underlying(hook)].name,
                                  param, group));

        EnvVarMap env_vars{ {"hook_param", param.str()} };
        for (size_t i = 0; i < captures.size(); ++i)
            env_vars.insert({format("hook_param_capture_{}", i),
                             {captures[i].first, captures[i].second}});
        for (auto& c : filter.impl()->named_captures)
            env_vars.insert({format("hook_param_capture_{}", c.name),
                             {captures[c.index].first, captures[c.index].second}});

        CommandManager::instance().execute(commands, context, {{}, std::move(env_vars)});
    }

};

HookManager::HookManager() : m_parent(nullptr) {}
HookManager::HookManager(HookManager& parent) : SafeCountable{}, m_parent(&parent) {}
HookManager::~HookManager() = default;

void HookManager::add_hook(Hook hook, String group, HookFlags flags, Regex filter, String commands, Context& context)
{
    UniquePtr<HookData> hook_data{new HookData{std::move(group), flags, std::move(filter), std::move(commands)}};
    if (hook == Hook::ModuleLoaded)
    {
        const bool only_always = context.hooks_disabled();
        auto& disabled_hooks = context.options()["disabled_hooks"].get<Regex>();

        for (auto&& name : CommandManager::instance().loaded_modules())
        {
            MatchResults<const char*> captures;
            if (hook_data->should_run(only_always, disabled_hooks, name, captures))
            {
                hook_data->exec(hook, name, context, captures);
                if (hook_data->flags & HookFlags::Once)
                    return;
            }
        }
    }
    m_hooks[to_underlying(hook)].push_back(std::move(hook_data));
}

void HookManager::remove_hooks(const Regex& regex)
{
    for (auto& list : m_hooks)
    {
        list.erase(remove_if(list, [this, &regex](UniquePtr<HookData>& h) {
                       if (not regex_match(h->group.begin(), h->group.end(), regex))
                           return false;
                       m_hooks_trash.push_back(std::move(h));
                       return true;
                   }), list.end());
    }
}

CandidateList HookManager::complete_hook_group(StringView prefix, ByteCount pos_in_token)
{
    CandidateList res;
    for (auto& list : m_hooks)
    {
        auto container = list | transform([](const UniquePtr<HookData>& h) -> const String& { return h->group; });
        for (auto& c : complete(prefix, pos_in_token, container))
        {
            if (!contains(res, c))
                res.push_back(c);
        }
    }
    return res;
}

void HookManager::run_hook(Hook hook, StringView param, Context& context)
{
    const bool only_always = context.hooks_disabled();
    auto& disabled_hooks = context.options()["disabled_hooks"].get<Regex>();

    struct ToRun { HookData* hook; MatchResults<const char*> captures; };
    Vector<ToRun> hooks_to_run; // The m_hooks_trash vector ensure hooks wont die during this method
    for (auto& hook : m_hooks[to_underlying(hook)])
    {
        MatchResults<const char*> captures;
        if (hook->should_run(only_always, disabled_hooks, param, captures))
            hooks_to_run.push_back({hook.get(), std::move(captures)});
    }

    if (m_parent)
        m_parent->run_hook(hook, param, context);

    auto hook_name = enum_desc(Meta::Type<Hook>{})[to_underlying(hook)].name;
    if (contains(m_running_hooks, std::make_pair(hook, param)))
    {
        auto error = format("recursive call of hook {}/{}, not executing", hook_name, param);
        write_to_debug_buffer(error);
        return;
    }

    m_running_hooks.emplace_back(hook, param);
    auto pop_running_hook = OnScopeEnd([this]{
        m_running_hooks.pop_back();
        if (m_running_hooks.empty())
            m_hooks_trash.clear();
    });

    ProfileScope profile{context, [&](std::chrono::microseconds duration) {
        write_to_debug_buffer(format("hook '{}({})' took {} us", hook_name, param, (size_t)duration.count()));
    }};

    bool hook_error = false;
    for (auto& to_run : hooks_to_run)
    {
        try
        {
            to_run.hook->exec(hook, param, context, to_run.captures);

            if (to_run.hook->flags & HookFlags::Once)
            {
                auto& hook_list = m_hooks[to_underlying(hook)];
                if (auto it = find(hook_list, to_run.hook); it != hook_list.end())
                {
                    m_hooks_trash.push_back(std::move(*it));
                    hook_list.erase(it);
                }
            }
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
                   hook_name, param), context.faces()["Error"] });
}

}
