#ifndef alias_registry_hh_INCLUDED
#define alias_registry_hh_INCLUDED

#include "safe_ptr.hh"
#include "string.hh"
#include "id_map.hh"

namespace Kakoune
{

class AliasRegistry : public SafeCountable
{
public:
    AliasRegistry(AliasRegistry& parent) : m_parent(&parent) {}
    void add_alias(String alias, String command);
    void remove_alias(StringView alias);
    StringView operator[](StringView name) const;

    using AliasMap = IdMap<String, MemoryDomain::Aliases>;
    using iterator = AliasMap::const_iterator;
    iterator begin() const { return m_aliases.begin(); }
    iterator end() const { return m_aliases.end(); }

    Vector<StringView> aliases_for(StringView command) const;

private:
    friend class Scope;
    AliasRegistry() {}

    SafePtr<AliasRegistry> m_parent;
    AliasMap m_aliases;
};

}

#endif // alias_registry_hh_INCLUDED
