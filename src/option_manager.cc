#include "option_manager.hh"
#include "assert.hh"

#include <sstream>

namespace Kakoune
{

Option::Option(OptionManager& manager, String name)
    : m_manager(manager), m_name(std::move(name)) {}

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

Option& OptionManager::get_local_option(const String& name)
{
    auto it = find_option(m_options, name);
    if (it != m_options.end())
        return **it;
    else if (m_parent)
    {
        m_options.emplace_back((*m_parent)[name].clone(*this));
        return *m_options.back();
    }
    else
        throw option_not_found(name);

}

const Option& OptionManager::operator[](const String& name) const
{
    auto it = find_option(m_options, name);
    if (it != m_options.end())
        return **it;
    else if (m_parent)
        return (*m_parent)[name];
    else
        throw option_not_found(name);
}

CandidateList OptionManager::complete_option_name(const String& prefix,
                                                  ByteCount cursor_pos)
{
    String real_prefix = prefix.substr(0, cursor_pos);
    CandidateList result;
    if (m_parent)
        result = m_parent->complete_option_name(prefix, cursor_pos);
    for (auto& option : m_options)
    {
        const auto& name = option->name();
        if (name.substr(0, real_prefix.length()) == real_prefix and
            not contains(result, name))
            result.push_back(name);
    }
    return result;
}

OptionManager::OptionList OptionManager::flatten_options() const
{
    OptionList res = m_parent ? m_parent->flatten_options() : OptionList{};
    for (auto& option : m_options)
    {
        auto it = find_option(res, option->name());
        if (it != res.end())
            *it = option.get();
        else
            res.emplace_back(option.get());
    }
    return res;
}

void OptionManager::on_option_changed(const Option& option)
{
    // if parent option changed, but we overrided it, it's like nothing happened
    if (&option.manager() != this and
        find_option(m_options, option.name()) != m_options.end())
        return;

    for (auto watcher : m_watchers)
        watcher->on_option_changed(option);
}

GlobalOptions::GlobalOptions()
    : OptionManager()
{
    declare_option<int>("tabstop", 8);
    declare_option<int>("indentwidth", 4);
    declare_option<String>("eolformat", "lf");
    declare_option<String>("BOM", "no");
    declare_option<String>("shell", "sh");
    declare_option<bool>("complete_prefix", true);
    declare_option<bool>("incsearch", true);
    declare_option<Regex>("ignored_files", Regex{R"(^(\..*|.*\.(o|so|a))$)"});
    declare_option<String>("filetype", "");
    declare_option<std::vector<String>>("completions", {});
    declare_option<std::vector<String>>("path", { "./", "/usr/include" });
    declare_option<bool>("insert_hide_sel", false);
}

}
