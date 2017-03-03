#ifndef keymap_manager_hh_INCLUDED
#define keymap_manager_hh_INCLUDED

#include "array_view.hh"
#include "keys.hh"
#include "hash.hh"
#include "string.hh"
#include "unordered_map.hh"
#include "vector.hh"

namespace Kakoune
{

enum class KeymapMode : char
{
    None,
    Normal,
    Insert,
    Prompt,
    Menu,
    Goto,
    View,
    User,
    Object,
};

class KeymapManager
{
public:
    KeymapManager(KeymapManager& parent) : m_parent(&parent) {}

    using KeyList = Vector<Key, MemoryDomain::Mapping>;
    void map_key(Key key, KeymapMode mode, KeyList mapping, String docstring);
    void unmap_key(Key key, KeymapMode mode);

    bool is_mapped(Key key, KeymapMode mode) const;
    KeyList get_mapped_keys(KeymapMode mode) const;

    struct KeyMapInfo
    {
        KeyList keys;
        String docstring;
    };
    const KeyMapInfo& get_mapping(Key key, KeymapMode mode) const;

private:
    KeymapManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root map manager
    friend class Scope;

    KeymapManager* m_parent;
    using KeyAndMode = std::pair<Key, KeymapMode>;
    UnorderedMap<KeyAndMode, KeyMapInfo, MemoryDomain::Mapping> m_mapping;
};

}

#endif // keymap_manager_hh_INCLUDED
