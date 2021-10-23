#ifndef keymap_manager_hh_INCLUDED
#define keymap_manager_hh_INCLUDED

#include "array_view.hh"
#include "keys.hh"
#include "hash.hh"
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

    using KeyList = Vector<Key, MemoryDomain::Mapping>;
    void map_key(Key key, KeymapMode mode, KeyList mapping, String docstring);
    void unmap_key(Key key, KeymapMode mode);
    void unmap_keys(KeymapMode mode);

    bool is_mapped(Key key, KeymapMode mode) const;
    KeyList get_mapped_keys(KeymapMode mode) const;

    struct KeymapInfo
    {
        KeyList keys;
        String docstring;
    };
    const KeymapInfo& get_mapping(Key key, KeymapMode mode) const;

    using UserModeList = Vector<String>;
    UserModeList& user_modes() {
        if (m_parent)
            return m_parent->user_modes();
        return m_user_modes;
    }
    void add_user_mode(String user_mode_name);

private:
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
