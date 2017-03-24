#ifndef alias_registry_hh_INCLUDED
#define alias_registry_hh_INCLUDED

#include "safe_ptr.hh"
#include "string.hh"
#include "hash_map.hh"

namespace Kakoune
{

class AliasRegistry : public SafeCountable
{
public:
    AliasRegistry(AliasRegistry& parent) : m_parent(&parent) {}
    void add_alias(String alias, String command);
    void remove_alias(StringView alias);
    StringView operator[](StringView alias) const;

    Vector<StringView> aliases_for(StringView command) const;

private:
    friend class Scope;
    AliasRegistry() {}

    SafePtr<AliasRegistry> m_parent;
    HashMap<String, String, MemoryDomain::Aliases> m_aliases;
};

}

#endif // alias_registry_hh_INCLUDED
