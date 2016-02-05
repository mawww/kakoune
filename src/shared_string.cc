#include "shared_string.hh"
#include "buffer_utils.hh"

namespace Kakoune
{

StringDataPtr StringRegistry::intern(StringView str)
{
    auto it = m_strings.find(str);
    if (it == m_strings.end())
    {
        auto data = StringData::create(str);
        it = m_strings.emplace(data->strview(), data).first;
    }
    return it->second;
}

void StringRegistry::purge_unused()
{
    for (auto it = m_strings.begin(); it != m_strings.end(); )
    {
        if (it->second->refcount == 1)
            it = m_strings.erase(it);
        else
            ++it;
    }
}

void StringRegistry::debug_stats() const
{
    write_to_debug_buffer("Shared Strings stats:");
    size_t total_refcount = 0;
    size_t total_size = 0;
    size_t count = m_strings.size();
    for (auto& st : m_strings)
    {
        total_refcount += st.second->refcount - 1;
        total_size += (int)st.second->length;
    }
    write_to_debug_buffer(format("  data size: {}, mean: {}", total_size, (float)total_size/count));
    write_to_debug_buffer(format("  refcounts: {}, mean: {}", total_refcount, (float)total_refcount/count));
}

}
