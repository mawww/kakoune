#include "interned_string.hh"
#include "debug.hh"

namespace Kakoune
{

void StringRegistry::debug_stats() const
{
    write_debug("Interned Strings stats:");
    write_debug("  slots: " + to_string(m_storage.size()) + " allocated, " + to_string(m_free_slots.size()) + " free");
    size_t total_refcount = 0;
    size_t total_size = 0;
    size_t count = 0;
    for (auto& st : m_storage)
    {
        if (st.refcount == 0)
            continue;
        total_refcount += st.refcount;
        total_size += st.data.size();
        ++count;
    }
    write_debug("  data size: " + to_string(total_size) + ", mean: " + to_string((float)total_size/count));
    write_debug("  refcounts: " + to_string(total_refcount) + ", mean: " + to_string((float)total_refcount/count));
}

InternedString StringRegistry::acquire(StringView str)
{
    auto it = m_slot_map.find(str);
    if (it == m_slot_map.end())
    {
        Slot slot;
        if (not m_free_slots.empty())
        {
            slot = m_free_slots.back();
            m_free_slots.pop_back();
            kak_assert(m_storage[slot].refcount == 0);
            m_storage[slot] = DataAndRefCount{{str.begin(), str.end()}, 1};
        }
        else
        {
            slot = m_storage.size();
            m_storage.push_back({{str.begin(), str.end()}, 1});
        }
        // Create a new string view that point to the storage data
        StringView storage_view{m_storage[slot].data.data(), (int)m_storage[slot].data.size()};
        m_slot_map[storage_view] = slot;

        return InternedString{storage_view, slot};
    }

    Slot slot = it->second;
    auto& data = m_storage[slot];
    ++data.refcount;
    return {{data.data.data(), (int)data.data.size()}, slot};
}

void StringRegistry::acquire(Slot slot)
{
    kak_assert(slot < m_storage.size());
    kak_assert(m_storage[slot].refcount > 0);
    ++m_storage[slot].refcount;
}

void StringRegistry::release(Slot slot) noexcept
{
    kak_assert(m_storage[slot].refcount > 0);
    if (--m_storage[slot].refcount == 0)
    {
        m_free_slots.push_back(slot);
        auto& data = m_storage[slot].data;
        auto it = m_slot_map.find(StringView{data.data(), (int)data.size()});
        kak_assert(it != m_slot_map.end());
        m_slot_map.erase(it);
        data = Vector<char, MemoryDomain::InternedString>{};
    }
}

}
