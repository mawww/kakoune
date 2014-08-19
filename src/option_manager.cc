#include "option_manager.hh"

#include "insert_completer.hh"

#include "assert.hh"

#include <sstream>

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

void OptionManager::register_watcher(OptionManagerWatcher& watcher)
{
    kak_assert(not contains(m_watchers, &watcher));
    m_watchers.push_back(&watcher);
}

void OptionManager::unregister_watcher(OptionManagerWatcher& watcher)
{
    auto it = find(m_watchers.begin(), m_watchers.end(), &watcher);
    kak_assert(it != m_watchers.end());
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

Option& OptionManager::operator[](const String& name)
{
    auto it = find_option(m_options, name);
    if (it != m_options.end())
        return **it;
    else if (m_parent)
        return (*m_parent)[name];
    else
        throw option_not_found(name);
}

const Option& OptionManager::operator[](const String& name) const
{
    return const_cast<OptionManager&>(*this)[name];
}

template<typename MatchingFunc>
CandidateList OptionManager::get_matching_names(MatchingFunc func)
{
    CandidateList result;
    if (m_parent)
        result = m_parent->get_matching_names(func);
    for (auto& option : m_options)
    {
        if (option->flags() & OptionFlags::Hidden)
            continue;

        const auto& name = option->name();
        if (func(name) and not contains(result, name))
            result.push_back(name);
    }
    return result;
}

CandidateList OptionManager::complete_option_name(StringView prefix,
                                                  ByteCount cursor_pos)
{
    using namespace std::placeholders;
    auto real_prefix = prefix.substr(0, cursor_pos);
    auto result = get_matching_names(std::bind(prefix_match, _1, real_prefix));
    if (result.empty())
        result = get_matching_names(std::bind(subsequence_match, _1, real_prefix));
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
    declare_option("tabstop", "size of a tab character", 8);
    declare_option("indentwidth", "indentation width", 4);
    declare_option("scrolloff",
                   "number of lines to keep visible main cursor when scrolling",
                   0);
    declare_option("eolformat", "end of line format: 'crlf' or 'lf'", "lf"_str);
    declare_option("BOM", "insert a byte order mark when writing buffer",
                   "no"_str);
    declare_option("complete_prefix",
                   "complete up to common prefix in tab completion",
                   true);
    declare_option("incsearch",
                   "incrementaly apply search/select/split regex",
                   true);
    declare_option("autoinfo",
                   "automatically display contextual help",
                   1);
    declare_option("autoshowcompl",
                   "automatically display possible completions for prompts",
                   true);
    declare_option("aligntab",
                   "use tab characters when possible for alignement",
                   false);
    declare_option("ignored_files",
                   "patterns to ignore when completing filenames",
                   Regex{R"(^(\..*|.*\.(o|so|a))$)"});
    declare_option("filetype", "buffer filetype", ""_str);
    declare_option("path", "path to consider when trying to find a file",
                   std::vector<String>({ "./", "/usr/include" }));
    declare_option("completers", "insert mode completers to execute.",
                    std::vector<InsertCompleterDesc>({
                        InsertCompleterDesc{ InsertCompleterDesc::Filename },
                        InsertCompleterDesc{ InsertCompleterDesc::Word, "all"_str }
                    }), OptionFlags::None);
    declare_option("autoreload",
                   "autoreload buffer when a filesystem modification is detected",
                    Ask);
}

}
