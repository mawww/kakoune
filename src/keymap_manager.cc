#include "keymap_manager.hh"

#include "array_view.hh"
#include "assert.hh"
#include "exception.hh"
#include "string_utils.hh"

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
    m_mapping.remove(KeyAndMode{key, mode});
}

void KeymapManager::unmap_keys(KeymapMode mode)
{
    auto it = m_mapping.begin();
    while (it != m_mapping.end())
    {
        auto& map = *it;
        if (map.key.second == mode)
            unmap_key(map.key.first, map.key.second);
        else
            ++it;
    }
}

bool KeymapManager::is_mapped(Key key, KeymapMode mode) const
{
    return m_mapping.find(KeyAndMode{key, mode}) != m_mapping.end() or
           (m_parent and m_parent->is_mapped(key, mode));
}

const KeymapManager::KeymapInfo&
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
        if (map.key.second == mode and not contains(res, map.key.first))
            res.emplace_back(map.key.first);
    }
    return res;
}

void KeymapManager::add_user_mode(String user_mode_name)
{
    auto modes = {"normal", "insert", "prompt", "menu", "goto", "view", "user", "object"};

    if (contains(modes, user_mode_name))
        throw runtime_error(format("'{}' is already a regular mode", user_mode_name));

    if (contains(user_modes(), user_mode_name))
        throw runtime_error(format("user mode '{}' already defined", user_mode_name));

    if (not all_of(user_mode_name, is_identifier))
        throw runtime_error(format("invalid mode name: '{}'", user_mode_name));

    user_modes().push_back(std::move(user_mode_name));
}

}
