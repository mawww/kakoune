#include "shared_string.hh"
#include "debug.hh"

namespace Kakoune
{

SharedString StringRegistry::intern(StringView str)
{
    auto it = m_strings.find(str);
    if (it == m_strings.end())
    {
        SharedString shared_str = str;
        it = m_strings.emplace(StringView{shared_str}, shared_str.m_storage).first;
    }
    return {*it->second, it->second};
}

void StringRegistry::purge_unused()
{
    for (auto it = m_strings.begin(); it != m_strings.end(); )
    {
        if (it->second.unique())
            it = m_strings.erase(it);
        else
            ++it;
    }
}

void StringRegistry::debug_stats() const
{

    write_debug("Shared Strings stats:");
    size_t total_refcount = 0;
    size_t total_size = 0;
    size_t count = m_strings.size();
    for (auto& st : m_strings)
    {
        total_refcount += st.second.use_count() - 1;
        total_size += (int)st.second->length();
    }
    write_debug("  data size: " + to_string(total_size) + ", mean: " + to_string((float)total_size/count));
    write_debug("  refcounts: " + to_string(total_refcount) + ", mean: " + to_string((float)total_refcount/count));
}

}
