#include "alias_registry.hh"

#include "command_manager.hh"
#include "ranges.hh"

namespace Kakoune
{

void AliasRegistry::add_alias(String alias, String command)
{
    kak_assert(not alias.empty());
    kak_assert(CommandManager::instance().command_defined(command));
    auto it = m_aliases.find(alias);
    if (it == m_aliases.end())
        m_aliases.insert({std::move(alias), std::move(command) });
    else
        it->value = std::move(command);
}

void AliasRegistry::remove_alias(StringView alias)
{
    m_aliases.remove(alias);
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
            res.emplace_back(alias.key);
    }

    return res;
}

}
