#include "option_manager.hh"
#include "assert.hh"

#include <sstream>

namespace Kakoune
{

OptionManager::OptionManager(OptionManager& parent)
    : m_parent(&parent)
{
    parent.register_watcher(*this);
}

OptionManager::~OptionManager()
{
    if (m_parent)
        m_parent->unregister_watcher(*this);

    assert(m_watchers.empty());
}

void OptionManager::register_watcher(OptionManagerWatcher& watcher)
{
    assert(not contains(m_watchers, &watcher));
    m_watchers.push_back(&watcher);
}

void OptionManager::unregister_watcher(OptionManagerWatcher& watcher)
{
    auto it = find(m_watchers.begin(), m_watchers.end(), &watcher);
    assert(it != m_watchers.end());
    m_watchers.erase(it);
}

void OptionManager::set_option(const String& name, const Option& value)
{
    Option old_value = m_options[name];
    m_options[name] = value;

    if (old_value != value)
    {
        for (auto watcher : m_watchers)
            watcher->on_option_changed(name, value);
    }
}

const Option& OptionManager::operator[](const String& name) const
{
    auto it = m_options.find(name);
    if (it != m_options.end())
        return it->second;
    else if (m_parent)
        return (*m_parent)[name];
    else
        throw option_not_found(name);
}

CandidateList OptionManager::complete_option_name(const String& prefix,
                                                  CharCount cursor_pos)
{
    String real_prefix = prefix.substr(0, cursor_pos);
    CandidateList result;
    if (m_parent)
        result = m_parent->complete_option_name(prefix, cursor_pos);
    for (auto& option : m_options)
    {
        if (option.first.substr(0, real_prefix.length()) == real_prefix and
            not contains(result, option.first))
            result.push_back(option.first);
    }
    return result;
}

OptionManager::OptionMap OptionManager::flatten_options() const
{
    OptionMap res = m_parent ? m_parent->flatten_options() : OptionMap();
    for (auto& option : m_options)
        res.insert(option);
    return res;
}

void OptionManager::on_option_changed(const String& name, const Option& value)
{
    // if parent option changed, but we overrided it, it's like nothing happened
    if (m_options.find(name) != m_options.end())
        return;

    for (auto watcher : m_watchers)
        watcher->on_option_changed(name, value);
}

GlobalOptionManager::GlobalOptionManager()
    : OptionManager()
{
    set_option("tabstop", Option(8));
    set_option("eolformat", Option("lf"));
    set_option("BOM", Option("no"));
}

}
