#ifndef keymap_manager_hh_INCLUDED
#define keymap_manager_hh_INCLUDED

#include "keys.hh"
#include "string.hh"
#include "hash_map.hh"
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
    FirstUserMode,
};

class KeymapManager
{
public:
    KeymapManager(KeymapManager& parent) : m_parent(&parent) {}

    void reparent(KeymapManager& parent) { m_parent = &parent; }

    using KeyList = Vector<Key, MemoryDomain::Mapping>;
    void map_key(Key key, KeymapMode mode, KeyList mapping, String docstring);
    void unmap_key(Key key, KeymapMode mode);
    void unmap_keys(KeymapMode mode);

    bool is_mapped(Key key, KeymapMode mode) const;
    KeyList get_mapped_keys(KeymapMode mode) const;

    auto get_mapping_keys(Key key, KeymapMode mode) {
        return get_mapping(key, mode).keys;
    }

    const String& get_mapping_docstring(Key key, KeymapMode mode) { return get_mapping(key, mode).docstring; }

    using UserModeList = Vector<String>;
    UserModeList& user_modes() {
        if (m_parent)
            return m_parent->user_modes();
        return m_user_modes;
    }
    void add_user_mode(String user_mode_name);

private:
    struct KeymapInfo
    {
        KeyList keys;
        String docstring;
    };
    const KeymapInfo& get_mapping(Key key, KeymapMode mode) const;

    KeymapManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root map manager
    friend class Scope;

    KeymapManager* m_parent;
    using KeyAndMode = std::pair<Key, KeymapMode>;
    HashMap<KeyAndMode, KeymapInfo, MemoryDomain::Mapping> m_mapping;

    UserModeList m_user_modes;
};

}

#endif // keymap_manager_hh_INCLUDED
