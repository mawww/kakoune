#ifndef shared_string_hh_INCLUDED
#define shared_string_hh_INCLUDED

#include "string.hh"
#include "ref_ptr.hh"
#include "utils.hh"
#include "unordered_map.hh"

namespace Kakoune
{

struct StringData : UseMemoryDomain<MemoryDomain::SharedString>
{
    int refcount;
    int length;
    uint32_t hash;

    StringData(int ref, int len) : refcount(ref), length(len) {}

    [[gnu::always_inline]]
    char* data() { return reinterpret_cast<char*>(this + 1); }
    [[gnu::always_inline]]
    const char* data() const { return reinterpret_cast<const char*>(this + 1); }
    [[gnu::always_inline]]
    StringView strview() const { return {data(), length}; }

    struct PtrPolicy
    {
        static void inc_ref(StringData* r, void*) { ++r->refcount; }
        static void dec_ref(StringData* r, void*) { if (--r->refcount == 0) delete r; }
        static void ptr_moved(StringData*, void*, void*) noexcept {}
    };

    static RefPtr<StringData, PtrPolicy> create(StringView str, char back = 0)
    {
        const int len = (int)str.length() + (back != 0 ? 1 : 0);
        void* ptr = StringData::operator new(sizeof(StringData) + len + 1);
        StringData* res = new (ptr) StringData(0, len);
        std::copy(str.begin(), str.end(), res->data());
        if (back != 0)
            res->data()[len-1] = back;
        res->data()[len] = 0;
        res->hash = hash_data(res->data(), res->length);
        return RefPtr<StringData, PtrPolicy>{res};
    }

    static void destroy(StringData* s)
    {
        StringData::operator delete(s, sizeof(StringData) + s->length + 1);
    }

    friend void inc_ref_count(StringData* s, void*)
    {
        ++s->refcount;
    }

    friend void dec_ref_count(StringData* s, void*)
    {
        if (--s->refcount == 0)
            StringData::destroy(s);
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
