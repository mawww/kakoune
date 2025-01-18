#include "shared_string.hh"
#include "debug.hh"
#include "format.hh"

namespace Kakoune
{

StringDataPtr StringData::Registry::intern(StringView str, size_t hash)
{
    kak_assert(hash_value(str) == hash);
    auto index = m_strings.find_index(str, hash);
    if (index >= 0)
        return StringDataPtr{m_strings.item(index).value};

    auto data = StringData::create(str);
    data->refcount |= interned_flag;
    m_strings.insert({data->strview(), data.get()}, hash);
    return data;
}

StringDataPtr StringData::Registry::intern(StringView str)
{
    return intern(str, hash_value(str));
}

void StringData::Registry::remove(StringView str)
{
    kak_assert(m_strings.contains(str));
    m_strings.unordered_remove(str);
}

void StringData::Registry::debug_stats() const
{
    write_to_debug_buffer("Interned Strings stats:");
    size_t count = m_strings.size();
    size_t total_refcount = 0;
    size_t total_size = 0;
    for (auto& st : m_strings)
    {
        total_refcount += st.value->refcount & refcount_mask;
        total_size += (int)st.value->length;
    }
    write_to_debug_buffer(format("  count: {}", count));
    write_to_debug_buffer(format("  data size: {}, mean: {}", total_size, (float)total_size/count));
    write_to_debug_buffer(format("  refcounts: {}, mean: {}", total_refcount, (float)total_refcount/count));
}

}
