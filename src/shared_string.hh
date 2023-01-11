#ifndef shared_string_hh_INCLUDED
#define shared_string_hh_INCLUDED

#include "string.hh"
#include "ref_ptr.hh"
#include "utils.hh"
#include "hash_map.hh"

#include <numeric>

namespace Kakoune
{

struct StringData : UseMemoryDomain<MemoryDomain::SharedString>
{
    uint32_t refcount;
    const int length;

    [[gnu::always_inline]]
    const char* data() const { return reinterpret_cast<const char*>(this + 1); }
    [[gnu::always_inline]]
    StringView strview() const { return {data(), length}; }

private:
    StringData(int len) : refcount(0), length(len) {}

    static constexpr uint32_t interned_flag = 1u << 31;
    static constexpr uint32_t refcount_mask = ~interned_flag;

    struct PtrPolicy
    {
        static void inc_ref(StringData* r, void*) noexcept { ++r->refcount; }
        static void dec_ref(StringData* r, void*) noexcept
        {
            if ((--r->refcount & refcount_mask) == 0)
            {
                if (r->refcount & interned_flag)
                    Registry::instance().remove(r->strview());
                StringData::operator delete(r, sizeof(StringData) + r->length + 1);
            }
        }
        static void ptr_moved(StringData*, void*, void*) noexcept {}
    };

public:
    using Ptr = RefPtr<StringData, PtrPolicy>;

    class Registry : public Singleton<Registry>
    {
    public:
        void debug_stats() const;
        Ptr intern(StringView str);
        void remove(StringView str);

    private:
        HashMap<StringView, StringData*, MemoryDomain::SharedString> m_strings;
    };

    static Ptr create(ArrayView<const StringView> strs);
};

using StringDataPtr = StringData::Ptr;
using StringRegistry = StringData::Registry;

inline StringDataPtr intern(StringView str)
{
    return StringRegistry::instance().intern(str);
}

}

#endif // shared_string_hh_INCLUDED
