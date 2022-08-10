#ifndef hash_map_hh_INCLUDED
#define hash_map_hh_INCLUDED

#include "hash.hh"
#include "memory.hh"
#include "vector.hh"

namespace Kakoune
{

template<typename T>
constexpr void constexpr_swap(T& lhs, T& rhs)
{
    T tmp = std::move(lhs);
    lhs = std::move(rhs);
    rhs = std::move(tmp);
}

template<MemoryDomain domain,
         template<typename, MemoryDomain> class Container>
struct HashIndex
{
    struct Entry
    {
        size_t hash = 0;
        int index = -1;
    };

    static constexpr float max_fill_rate = 0.5f;

    constexpr HashIndex() = default;
    constexpr HashIndex(size_t count)
    {
        const size_t min_size = (size_t)(count / max_fill_rate) + 1;
        size_t new_size = 4;
        while (new_size < min_size)
            new_size *= 2;
        m_entries.resize(new_size);
    }

    using ContainerType = Container<Entry, domain>;

    constexpr void resize(size_t new_size)
    {
        kak_assert(new_size > m_entries.size());
        ContainerType old_entries = std::move(m_entries);
        m_entries.resize(new_size);
        for (auto& entry : old_entries)
        {
            if (entry.index >= 0)
                add(entry.hash, entry.index);
        }
    }

    constexpr void reserve(size_t count)
    {
        if (count == 0)
            return;

        const size_t min_size = (size_t)(count / max_fill_rate) + 1;
        size_t new_size = m_entries.empty() ? 4 : m_entries.size();
        while (new_size < min_size)
            new_size *= 2;

        if (new_size > m_entries.size())
            resize(new_size);
    }

    constexpr void add(size_t hash, int index)
    {
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
                    constexpr_swap(m_entries[slot], entry);
                    target_slot = candidate_slot;
                }
            }
            // no free entries found, resize, try again
            resize(m_entries.size() * 2);
        }
    }

    constexpr void remove(size_t hash, int index)
    {
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
                    constexpr_swap(m_entries[next-1], m_entries[next]);
                }
                break;
            }
        }
    }

    constexpr void ordered_fix_entries(int index)
    {
        for (auto& entry : m_entries)
        {
            if (entry.index >= index)
                --entry.index;
        }
    }

    constexpr void unordered_fix_entries(size_t hash, int old_index, int new_index)
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

    constexpr const Entry& operator[](size_t index) const { return m_entries[index]; }
    constexpr size_t size() const { return m_entries.size(); }
    constexpr size_t compute_slot(size_t hash) const
    {
        // We assume entries.size() is power of 2
        return hash & (m_entries.size()-1);
    }

    constexpr void clear() { m_entries.clear(); }

private:
    ContainerType m_entries;
};

template<typename Key, typename Value>
struct HashItem
{
    Key key{};
    Value value{};

    friend bool operator==(const HashItem&, const HashItem&) = default;
};

template<typename Key>
struct HashItem<Key, void>
{
    Key key;

    friend bool operator==(const HashItem&, const HashItem&) = default;
};

template<typename Key, typename Value,
         MemoryDomain domain = MemoryDomain::Undefined,
         template<typename, MemoryDomain> class Container = Vector,
         bool multi_key = false>
struct HashMap
{
    static constexpr bool has_value = not std::is_void_v<Value>;
    using Item = std::conditional_t<has_value, HashItem<Key, Value>, Key>;
    using EffectiveValue = std::conditional_t<has_value, Value, const Key>;
    using ContainerType = Container<Item, domain>;

    constexpr HashMap() = default;

    constexpr HashMap(std::initializer_list<Item> val) : m_items(val), m_index(val.size())
    {
        for (int i = 0; i < m_items.size(); ++i)
            m_index.add(hash_value(m_items[i].key), i);
    }

    template<typename Iterator>
    constexpr HashMap(Iterator begin, Iterator end)
    {
        while (begin != end)
            insert(*begin++);
    }

    constexpr EffectiveValue& insert(Item item)
    {
        const auto hash = hash_value(item_key(item));
        if constexpr (not multi_key)
        {
            if (auto index = find_index(item_key(item), hash); index >= 0)
            {
                m_items[index] = std::move(item);
                return item_value(m_items[index]);
            }
        }

        m_index.reserve(m_items.size()+1);
        m_index.add(hash, (int)m_items.size());
        m_items.push_back(std::move(item));
        return item_value(m_items.back());
    }

    template<typename KeyType> requires IsHashCompatible<Key, KeyType>
    constexpr int find_index(const KeyType& key, size_t hash) const
    {
        for (auto slot = m_index.compute_slot(hash); slot < m_index.size(); ++slot)
        {
            auto& entry = m_index[slot];
            if (entry.index == -1)
                return -1;
            if (entry.hash == hash and item_key(m_items[entry.index]) == key)
                return entry.index;
        }
        return -1;
    }

    template<typename KeyType> requires IsHashCompatible<Key, KeyType>
    constexpr int find_index(const KeyType& key) const { return find_index(key, hash_value(key)); }

    template<typename KeyType> requires IsHashCompatible<Key, KeyType>
    constexpr bool contains(const KeyType& key) const { return find_index(key) >= 0; }

    template<typename KeyType> requires IsHashCompatible<Key, std::remove_cvref_t<KeyType>>
    constexpr EffectiveValue& operator[](KeyType&& key) 
    {
        const auto hash = hash_value(key);
        auto index = find_index(key, hash);
        if (index >= 0)
            return item_value(m_items[index]);

        m_index.reserve(m_items.size()+1);
        m_index.add(hash, (int)m_items.size());
        m_items.push_back({Key(std::forward<KeyType>(key))});
        return item_value(m_items.back());
    }

    template<typename KeyType> requires IsHashCompatible<Key, std::remove_cvref_t<KeyType>>
    constexpr const EffectiveValue& get(KeyType&& key) const
    {
        return const_cast<HashMap&>(*this).get(key);
    }

    template<typename KeyType> requires IsHashCompatible<Key, std::remove_cvref_t<KeyType>>
    constexpr EffectiveValue& get(KeyType&& key)
    {
        const auto hash = hash_value(key);
        auto index = find_index(key, hash);
        kak_assert(index >= 0);
        return item_value(m_items[index]);
    }

    template<typename KeyType> requires IsHashCompatible<Key, KeyType>
    constexpr void remove(const KeyType& key)
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

    template<typename KeyType> requires IsHashCompatible<Key, KeyType>
    constexpr void unordered_remove(const KeyType& key)
    {
        const auto hash = hash_value(key);
        int index = find_index(key, hash);
        if (index >= 0)
        {
            constexpr_swap(m_items[index], m_items.back());
            m_items.pop_back();
            m_index.remove(hash, index);
            if (index != m_items.size())
                m_index.unordered_fix_entries(hash_value(item_key(m_items[index])), m_items.size(), index);
        }
    }

    template<typename KeyType> requires IsHashCompatible<Key, KeyType>
    constexpr void erase(const KeyType& key) { unordered_remove(key); }

    template<typename KeyType> requires IsHashCompatible<Key, KeyType>
    constexpr void remove_all(const KeyType& key)
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

    using iterator = typename ContainerType::iterator;
    constexpr iterator begin() { return m_items.begin(); }
    constexpr iterator end() { return m_items.end(); }

    using const_iterator = typename ContainerType::const_iterator;
    constexpr const_iterator begin() const { return m_items.begin(); }
    constexpr const_iterator end() const { return m_items.end(); }

    const Item& item(size_t index) const { return m_items[index]; }

    template<typename KeyType> requires IsHashCompatible<Key, KeyType>
    constexpr iterator find(const KeyType& key)
    {
        auto index = find_index(key);
        return index >= 0 ? begin() + index : end();
    }

    template<typename KeyType> requires IsHashCompatible<Key, KeyType>
    constexpr const_iterator find(const KeyType& key) const
    {
        return const_cast<HashMap*>(this)->find(key);
    }

    constexpr void clear() { m_items.clear(); m_index.clear(); }

    constexpr size_t size() const { return m_items.size(); }
    constexpr bool empty() const { return m_items.empty(); }
    constexpr void reserve(size_t size)
    {
        m_items.reserve(size);
        m_index.reserve(size);
    }

    // Equality is taking the order of insertion into account
    template<MemoryDomain otherDomain>
    constexpr bool operator==(const HashMap<Key, Value, otherDomain, Container>& other) const
    {
        return size() == other.size() and std::equal(begin(), end(), other.begin());
    }

    template<MemoryDomain otherDomain>
    constexpr bool operator!=(const HashMap<Key, Value, otherDomain, Container>& other) const
    {
        return not (*this == other);
    }

private:
    static auto& item_value(auto& item)
    {
        if constexpr (has_value) { return item.value; } else { return item; }
    }

    static const Key& item_key(const Item& item)
    {
        if constexpr (has_value) { return item.key; } else { return item; }
    }

    ContainerType m_items;
    HashIndex<domain, Container> m_index;
};

template<typename Key, typename Value,
         MemoryDomain domain = MemoryDomain::Undefined,
         template<typename, MemoryDomain> class Container = Vector>
 using MultiHashMap = HashMap<Key, Value, domain, Container, true>;

template<typename Value,
         MemoryDomain domain = MemoryDomain::Undefined,
         template<typename, MemoryDomain> class Container = Vector>
 using HashSet = HashMap<Value, void, domain, Container>;

template<typename Value,
         MemoryDomain domain = MemoryDomain::Undefined,
         template<typename, MemoryDomain> class Container = Vector>
 using MultiHashSet = HashMap<Value, void, domain, Container, true>;

void profile_hash_maps();

}

#endif // hash_map_hh_INCLUDED
