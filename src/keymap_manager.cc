#include "keymap_manager.hh"

#include "array_view.hh"
#include "assert.hh"

#include <algorithm>

namespace Kakoune
{

void KeymapManager::map_key(Key key, KeymapMode mode,
                            KeyList mapping, String docstring)
{
    m_mapping[KeyAndMode{key, mode}] = {std::move(mapping), std::move(docstring)};
}

void KeymapManager::unmap_key(Key key, KeymapMode mode)
{
    m_mapping.unordered_remove(KeyAndMode{key, mode});
}


bool KeymapManager::is_mapped(Key key, KeymapMode mode) const
{
    return m_mapping.find(KeyAndMode{key, mode}) != m_mapping.end() or
           (m_parent and m_parent->is_mapped(key, mode));
}

const KeymapManager::KeyMapInfo&
KeymapManager::get_mapping(Key key, KeymapMode mode) const
{
    auto it = m_mapping.find(KeyAndMode{key, mode});
    if (it != m_mapping.end())
        return it->value;
    kak_assert(m_parent);
    return m_parent->get_mapping(key, mode);
}

KeymapManager::KeyList KeymapManager::get_mapped_keys(KeymapMode mode) const
{
    KeyList res;
    if (m_parent)
        res = m_parent->get_mapped_keys(mode);
    for (auto& map : m_mapping)
    {
        if (map.key.second == mode)
            res.emplace_back(map.key.first);
    }
    std::sort(res.begin(), res.end());
    res.erase(std::unique(res.begin(), res.end()), res.end());
    return res;
}

}
