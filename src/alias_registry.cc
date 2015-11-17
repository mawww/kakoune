#include "alias_registry.hh"

#include "command_manager.hh"
#include "containers.hh"

namespace Kakoune
{

void AliasRegistry::add_alias(String alias, String command)
{
    kak_assert(not alias.empty());
    kak_assert(CommandManager::instance().command_defined(command));
    auto it = m_aliases.find(alias);
    if (it == m_aliases.end())
        m_aliases.append({std::move(alias), std::move(command) });
    else
        it->value = std::move(command);
}

void AliasRegistry::remove_alias(StringView alias)
{
    auto it = m_aliases.find(alias);
    if (it != m_aliases.end())
        m_aliases.erase(it);
}

StringView AliasRegistry::operator[](StringView alias) const
{
    auto it = m_aliases.find(alias);
    if (it != m_aliases.end())
        return it->value;
    else if (m_parent)
        return (*m_parent)[alias];
    else
        return StringView{};
}

Vector<StringView> AliasRegistry::aliases_for(StringView command) const
{
    Vector<StringView> res;
    if (m_parent)
        res = m_parent->aliases_for(command);

    for (auto& alias : m_aliases)
    {
        if (alias.value == command)
            res.push_back(alias.key);
    }

    return res;
}

Vector<std::pair<StringView, StringView>> AliasRegistry::flatten_aliases() const
{
    Vector<std::pair<StringView, StringView>> res;
    if (m_parent)
        res = m_parent->flatten_aliases();
    for (auto& alias : m_aliases)
    {
        if (not contains(transformed(res, [](const std::pair<StringView, StringView>& val) { return val.first; }), alias.key))
            res.emplace_back(alias.key, alias.value);
    }
    return res;
}

}
