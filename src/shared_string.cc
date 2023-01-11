#include "shared_string.hh"
#include "buffer_utils.hh"

#include <cstring>

namespace Kakoune
{

StringDataPtr StringData::create(ArrayView<const StringView> strs)
{
    const int len = accumulate(strs, 0, [](int l, StringView s) {
                        return l + (int)s.length();
                    });
    void* ptr = StringData::operator new(sizeof(StringData) + len + 1);
    auto* res = new (ptr) StringData(len);
    auto* data = reinterpret_cast<char*>(res + 1);
    for (auto& str : strs)
    {
        if (str.length() == 0) // memccpy(..., nullptr, 0) is UB
            continue;
        memcpy(data, str.begin(), (size_t)str.length());
        data += (int)str.length();
    }
    *data = 0;
    return RefPtr<StringData, PtrPolicy>{res};
}

StringDataPtr StringData::Registry::intern(StringView str)
{
    auto it = m_strings.find(str);
    if (it != m_strings.end())
        return StringDataPtr{it->value};

    auto data = StringData::create(str);
    data->refcount |= interned_flag;
    m_strings.insert({data->strview(), data.get()});
    return data;
}

void StringData::Registry::remove(StringView str)
{
    kak_assert(m_strings.contains(str));
    m_strings.unordered_remove(str);
}

void StringData::Registry::debug_stats() const
{
    write_to_debug_buffer("Shared Strings stats:");
    size_t total_refcount = 0;
    size_t total_size = 0;
    size_t count = m_strings.size();
    for (auto& st : m_strings)
    {
        total_refcount += (st.value->refcount & refcount_mask) - 1;
        total_size += (int)st.value->length;
    }
    write_to_debug_buffer(format("  data size: {}, mean: {}", total_size, (float)total_size/count));
    write_to_debug_buffer(format("  refcounts: {}, mean: {}", total_refcount, (float)total_refcount/count));
}

}
