#ifndef keymap_manager_hh_INCLUDED
#define keymap_manager_hh_INCLUDED

#include "keys.hh"
#include "utils.hh"

#include <unordered_map>
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
};

template<typename T> class memoryview;

class KeymapManager
{
public:
    KeymapManager(KeymapManager& parent) : m_parent(&parent) {}

    void map_key(Key key, KeymapMode mode, std::vector<Key> mapping);
    void unmap_key(Key key, KeymapMode mode);

    bool is_mapped(Key key, KeymapMode mode) const;
    memoryview<Key> get_mapping(Key key, KeymapMode mode) const;
private:
    KeymapManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root map manager
    friend class Scope;

    KeymapManager* m_parent;

    using KeyList = std::vector<Key>;
    using Keymap = std::unordered_map<std::pair<Key, KeymapMode>, KeyList>;
    Keymap m_mapping;
};

}

#endif // keymap_manager_hh_INCLUDED

