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
    AliasRegistry(AliasRegistry& parent) : SafeCountable{}, m_parent(&parent) {}
    void add_alias(String alias, String command);
    void remove_alias(StringView alias);
    StringView operator[](StringView alias) const;

    Vector<StringView> aliases_for(StringView command) const;

    auto flatten_aliases() const
    {
        auto merge = [](auto&& first, const AliasMap& second) {
            return concatenated(std::forward<decltype(first)>(first)
                                | filter([&second](auto& i) { return not second.contains(i.key); }),
                                second);
        };
        static const AliasMap empty;
        auto& parent = m_parent ? m_parent->m_aliases : empty;
        auto& grand_parent = (m_parent and m_parent->m_parent) ? m_parent->m_parent->m_aliases : empty;
        return merge(merge(grand_parent, parent), m_aliases);
    }

private:
    friend class Scope;
    AliasRegistry() = default;

    SafePtr<AliasRegistry> m_parent;
    using AliasMap = HashMap<String, String, MemoryDomain::Aliases>;
    AliasMap m_aliases;
};

}

#endif // alias_registry_hh_INCLUDED
