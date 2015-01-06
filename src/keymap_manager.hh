#ifndef keymap_manager_hh_INCLUDED
#define keymap_manager_hh_INCLUDED

#include "keys.hh"
#include "hash.hh"
#include "unordered_map.hh"

#include <vector>

namespace Kakoune
{

enum class KeymapMode : int
{
    None,
    Normal,
    Insert,
    Prompt,
    Menu,
    Goto,
    View,
    User,
};

template<typename T> class ArrayView;

class KeymapManager
{
public:
    KeymapManager(KeymapManager& parent) : m_parent(&parent) {}

    using KeyList = std::vector<Key>;
    void map_key(Key key, KeymapMode mode, KeyList mapping);
    void unmap_key(Key key, KeymapMode mode);

    bool is_mapped(Key key, KeymapMode mode) const;
    ArrayView<Key> get_mapping(Key key, KeymapMode mode) const;
private:
    KeymapManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root map manager
    friend class Scope;

    KeymapManager* m_parent;

    using KeyAndMode = std::pair<Key, KeymapMode>;
    using Keymap = UnorderedMap<KeyAndMode, KeyList>;
    Keymap m_mapping;
};

}

#endif // keymap_manager_hh_INCLUDED
