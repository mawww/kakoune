#ifndef shared_string_hh_INCLUDED
#define shared_string_hh_INCLUDED

#include "string.hh"
#include "ref_ptr.hh"
#include "utils.hh"
#include "unordered_map.hh"

#include <numeric>

namespace Kakoune
{

struct StringData : UseMemoryDomain<MemoryDomain::SharedString>
{
    int refcount;
    int length;

    StringData(int ref, int len) : refcount(ref), length(len) {}

    [[gnu::always_inline]]
    char* data() { return reinterpret_cast<char*>(this + 1); }
    [[gnu::always_inline]]
    const char* data() const { return reinterpret_cast<const char*>(this + 1); }
    [[gnu::always_inline]]
    StringView strview() const { return {data(), length}; }

    struct PtrPolicy
    {
        static void inc_ref(StringData* r, void*) noexcept { ++r->refcount; }
        static void dec_ref(StringData* r, void*) noexcept { if (--r->refcount == 0) destroy(r); }
        static void ptr_moved(StringData*, void*, void*) noexcept {}
    };

    static RefPtr<StringData, PtrPolicy> create(ArrayView<const StringView> strs)
    {
        const int len = std::accumulate(strs.begin(), strs.end(), 0,
                                        [](int l, StringView s)
                                        { return l + (int)s.length(); });
        void* ptr = StringData::operator new(sizeof(StringData) + len + 1);
        auto* res = new (ptr) StringData(0, len);
        auto* data = res->data();
        for (auto& str : strs)
        {
            memcpy(data, str.begin(), (size_t)str.length());
            data += (int)str.length();
        }
        res->data()[len] = 0;
        return RefPtr<StringData, PtrPolicy>{res};
    }

    static void destroy(StringData* s)
    {
        StringData::operator delete(s, sizeof(StringData) + s->length + 1);
    }
};

using StringDataPtr = RefPtr<StringData, StringData::PtrPolicy>;

class StringRegistry : public Singleton<StringRegistry>
{
public:
    void debug_stats() const;
    StringDataPtr intern(StringView str);
    void purge_unused();

private:
    UnorderedMap<StringView, StringDataPtr, MemoryDomain::SharedString> m_strings;
};

inline StringDataPtr intern(StringView str)
{
    return StringRegistry::instance().intern(str);
}

}

#endif // shared_string_hh_INCLUDED
