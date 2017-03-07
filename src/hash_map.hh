#ifndef hash_map_hh_INCLUDED
#define hash_map_hh_INCLUDED

#include "hash.hh"
#include "memory.hh"
#include "vector.hh"

namespace Kakoune
{

class String;

struct HashStats;

template<MemoryDomain domain>
struct HashIndex
{
    struct Entry
    {
        size_t hash;
        int index;
    };

    void grow()
    {
        Vector<Entry, domain> old_entries = std::move(m_entries);
        constexpr size_t init_size = 4;
        m_entries.resize(old_entries.empty() ? init_size : old_entries.size() * 2, {0,-1});
        for (auto& entry : old_entries)
        {
            if (entry.index >= 0)
                add(entry.hash, entry.index);
        }
    }

    void add(size_t hash, int index)
    {
        ++m_count;
        if ((float)m_count / m_entries.size() > m_max_fill_rate)
            grow();

        Entry entry{hash, index};
        while (true)
        {
            auto target_slot = compute_slot(entry.hash);
            for (auto slot = target_slot; slot < m_entries.size(); ++slot)
            {
                if (m_entries[slot].index == -1)
                {
                    m_entries[slot] = entry;
                    return;
                }

                // Robin hood hashing
                auto candidate_slot = compute_slot(m_entries[slot].hash);
                if (target_slot < candidate_slot)
                {
                    std::swap(m_entries[slot], entry);
                    target_slot = candidate_slot;
                }
            }
            // no free entries found, grow, try again
            grow();
        }
    }

    void remove(size_t hash, int index)
    {
        --m_count;
        for (auto slot = compute_slot(hash); slot < m_entries.size(); ++slot)
        {
            kak_assert(m_entries[slot].index >= 0);
            if (m_entries[slot].index == index)
            {
                m_entries[slot].index = -1;
                // Recompact following entries
                for (auto next = slot+1; next < m_entries.size(); ++next)
                {
                    if (m_entries[next].index == -1 or
                        compute_slot(m_entries[next].hash) == next)
                        break;
                    kak_assert(compute_slot(m_entries[next].hash) < next);
                    std::swap(m_entries[next-1], m_entries[next]);
                }
                break;
            }
        }
    }

    void ordered_fix_entries(int index)
    {
        // Fix entries index
        for (auto& entry : m_entries)
        {
            if (entry.index >= index)
                --entry.index;
        }
    }

    void unordered_fix_entries(size_t hash, int old_index, int new_index)
    {
        for (auto slot = compute_slot(hash); slot < m_entries.size(); ++slot)
        {
            if (m_entries[slot].index == old_index)
            {
                m_entries[slot].index = new_index;
                return;
            }
        }
        kak_assert(false); // entry not found ?!
    }

    const Entry& operator[](size_t index) const { return m_entries[index]; }
    size_t size() const { return m_entries.size(); }
    size_t compute_slot(size_t hash) const
    {
        // We assume entries.size() is power of 2
        return m_entries.empty() ? 0 : hash & (m_entries.size()-1);
    }

    void clear() { m_entries.clear(); }

    HashStats compute_stats() const;

private:
    size_t m_count = 0;
    float m_max_fill_rate = 0.5f;
    Vector<Entry, domain> m_entries;
};

template<typename Key, typename Value>
struct HashItem
{
    Key key;
    Value value;
};

template<typename Key, typename Value, MemoryDomain domain = MemoryDomain::Undefined>
struct HashMap
{
    using Item = HashItem<Key, Value>;

    HashMap() = default;

    HashMap(std::initializer_list<Item> val) : m_items{val}
    {
        for (int i = 0; i < m_items.size(); ++i)
            m_index.add(hash_value(m_items[i].key), i);
    }

    Value& insert(Item item)
    {
        m_index.add(hash_value(item.key), (int)m_items.size());
        m_items.push_back(std::move(item));
        return m_items.back().value;
    }

    template<typename KeyType>
    using EnableIfHashCompatible = typename std::enable_if<
        HashCompatible<Key, typename std::decay<KeyType>::type>::value
    >::type;

    template<typename KeyType, typename = EnableIfHashCompatible<KeyType>>
    int find_index(const KeyType& key, size_t hash) const
    {
        for (auto slot = m_index.compute_slot(hash); slot < m_index.size(); ++slot)
        {
            auto& entry = m_index[slot];
            if (entry.index == -1)
                return -1;
            if (entry.hash == hash and m_items[entry.index].key == key)
                return entry.index;
        }
        return -1;
    }

    template<typename KeyType, typename = EnableIfHashCompatible<KeyType>>
    int find_index(const KeyType& key) const { return find_index(key, hash_value(key)); }

    template<typename KeyType, typename = EnableIfHashCompatible<KeyType>>
    bool contains(const KeyType& key) const { return find_index(key) >= 0; }

    template<typename KeyType, typename = EnableIfHashCompatible<KeyType>>
    Value& operator[](KeyType&& key)
    {
        const auto hash = hash_value(key);
        auto index = find_index(key, hash);
        if (index >= 0)
            return m_items[index].value;

        m_index.add(hash, (int)m_items.size());
        m_items.push_back({Key{std::forward<KeyType>(key)}, {}});
        return m_items.back().value;
    }

    template<typename KeyType, typename = EnableIfHashCompatible<KeyType>>
    void remove(const KeyType& key)
    {
        const auto hash = hash_value(key);
        int index = find_index(key, hash);
        if (index >= 0)
        {
            m_items.erase(m_items.begin() + index);
            m_index.remove(hash, index);
            m_index.ordered_fix_entries(index);
        }
    }

    template<typename KeyType, typename = EnableIfHashCompatible<KeyType>>
    void unordered_remove(const KeyType& key)
    {
        const auto hash = hash_value(key);
        int index = find_index(key, hash);
        if (index >= 0)
        {
            std::swap(m_items[index], m_items.back());
            m_items.pop_back();
            m_index.remove(hash, index);
            if (index != m_items.size())
                m_index.unordered_fix_entries(hash_value(m_items[index].key), m_items.size(), index);
        }
    }

    void erase(const Key& key) { unordered_remove(key); }

    template<typename KeyType, typename = EnableIfHashCompatible<KeyType>>
    void remove_all(const KeyType& key)
    {
        const auto hash = hash_value(key);
        for (int index = find_index(key, hash); index >= 0;
             index = find_index(key, hash))
        {
            m_items.erase(m_items.begin() + index);
            m_index.remove(hash, index);
            m_index.ordered_fix_entries(index);
        }
    }

    using iterator = typename Vector<Item, domain>::iterator;
    iterator begin() { return m_items.begin(); }
    iterator end() { return m_items.end(); }

    using const_iterator = typename Vector<Item, domain>::const_iterator;
    const_iterator begin() const { return m_items.begin(); }
    const_iterator end() const { return m_items.end(); }

    template<typename KeyType, typename = EnableIfHashCompatible<KeyType>>
    iterator find(const KeyType& key)
    {
        auto index = find_index(key);
        return index >= 0 ? begin() + index : end();
    }

    template<typename KeyType, typename = EnableIfHashCompatible<KeyType>>
    const_iterator find(const KeyType& key) const
    {
        return const_cast<HashMap*>(this)->find(key);
    }

    void clear() { m_items.clear(); m_index.clear(); }

    size_t size() const { return m_items.size(); }
    bool empty() const { return m_items.empty(); }
    void reserve(size_t size)
    {
        m_items.reserve(size); 
        // TODO: Reserve in the index as well
    }

    // Equality is taking the order of insertion into account
    template<MemoryDomain otherDomain>
    bool operator==(const HashMap<Key, Value, otherDomain>& other) const
    {
        return size() == other.size() and
            std::equal(begin(), end(), other.begin(),
                       [](const Item& lhs, const Item& rhs) {
                           return lhs.key == rhs.key and lhs.value == rhs.value;
                       });
    }

    template<MemoryDomain otherDomain>
    bool operator!=(const HashMap<Key, Value, otherDomain>& other) const
    {
        return not (*this == other);
    }

    HashStats compute_stats() const;
private:
    Vector<Item, domain> m_items;
    HashIndex<domain> m_index;
};

void profile_hash_maps();

}

#endif // hash_map_hh_INCLUDED
