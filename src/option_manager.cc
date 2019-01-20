#include "option_manager.hh"

#include "assert.hh"
#include "flags.hh"
#include "scope.hh"

namespace Kakoune
{

OptionDesc::OptionDesc(String name, String docstring, OptionFlags flags)
    : m_name(std::move(name)), m_docstring(std::move(docstring)),
    m_flags(flags) {}

Option::Option(const OptionDesc& desc, OptionManager& manager)
    : m_manager(manager), m_desc(desc) {}

OptionManager::OptionManager(OptionManager& parent)
    : m_parent(&parent)
{
    parent.register_watcher(*this);
}

OptionManager::~OptionManager()
{
    if (m_parent)
        m_parent->unregister_watcher(*this);

    kak_assert(m_watchers.empty());
}

void OptionManager::register_watcher(OptionManagerWatcher& watcher) const
{
    kak_assert(not contains(m_watchers, &watcher));
    m_watchers.push_back(&watcher);
}

void OptionManager::unregister_watcher(OptionManagerWatcher& watcher) const
{
    auto it = find(m_watchers.begin(), m_watchers.end(), &watcher);
    kak_assert(it != m_watchers.end());
    m_watchers.erase(it);
}

struct option_not_found : public runtime_error
{
    option_not_found(StringView name)
        : runtime_error(format("option not found: '{}'. Use declare-option first", name)) {}
};

Option& OptionManager::get_local_option(StringView name)
{
    auto it = m_options.find(name);
    if (it != m_options.end())
        return *(it->value);
    else if (m_parent)
    {
        auto* clone = (*m_parent)[name].clone(*this);
        return *m_options.insert({clone->name(), std::unique_ptr<Option>{clone}});
    }
    else
        throw option_not_found(name);

}

Option& OptionManager::operator[](StringView name)
{
    auto it = m_options.find(name);
    if (it != m_options.end())
        return *it->value;
    else if (m_parent)
        return (*m_parent)[name];
    else
        throw option_not_found(name);
}

const Option& OptionManager::operator[](StringView name) const
{
    return const_cast<OptionManager&>(*this)[name];
}

void OptionManager::unset_option(StringView name)
{
    kak_assert(m_parent); // cannot unset option on global manager
    auto it = m_options.find(name);
    if (it != m_options.end())
    {
        auto& parent_option = (*m_parent)[name];
        const bool changed = not parent_option.has_same_value(*it->value);
        GlobalScope::instance().option_registry().move_to_trash(std::move(it->value));
        m_options.erase(name);
        if (changed)
            on_option_changed(parent_option);
    }
}

void OptionManager::on_option_changed(const Option& option)
{
    // if parent option changed, but we overrided it, it's like nothing happened
    if (&option.manager() != this and m_options.contains(option.name()))
        return;

    // The watcher list might get mutated during calls to on_option_changed
    auto watchers = m_watchers;
    for (auto* watcher : watchers)
    {
        if (contains(m_watchers, watcher)) // make sure this watcher is still alive
            watcher->on_option_changed(option);
    }
}

CandidateList OptionsRegistry::complete_option_name(StringView prefix,
                                                    ByteCount cursor_pos) const
{
    using OptionPtr = std::unique_ptr<const OptionDesc>;
    return complete(prefix, cursor_pos, m_descs |
                    filter([](const OptionPtr& desc)
                           { return not (desc->flags() & OptionFlags::Hidden); }) |
                    transform(&OptionDesc::name));
}

}
