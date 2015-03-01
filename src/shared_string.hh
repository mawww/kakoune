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

    StringData() = default;
    constexpr StringData(int refcount, int length) : refcount(refcount), length(length) {}

    [[gnu::always_inline]]
    char* data() { return reinterpret_cast<char*>(this + 1); }
    [[gnu::always_inline]]
    const char* data() const { return reinterpret_cast<const char*>(this + 1); }
    [[gnu::always_inline]]
    StringView strview() const { return {data(), length}; }

    static RefPtr<StringData> create(StringView str, char back = 0)
    {
        const int len = (int)str.length() + (back != 0 ? 1 : 0);
        void* ptr = StringData::operator new(sizeof(StringData) + len + 1);
        StringData* res = reinterpret_cast<StringData*>(ptr);
        std::copy(str.begin(), str.end(), res->data());
        res->refcount = 0;
        res->length = len;
        if (back != 0)
            res->data()[len-1] = back;
        res->data()[len] = 0;
        return RefPtr<StringData>(res);
    }

    static void destroy(StringData* s)
    {
        StringData::operator delete(s, sizeof(StringData) + s->length + 1);
    }

    friend void inc_ref_count(StringData* s, void*)
    {
        if (s->refcount != -1)
            ++s->refcount;
    }

    friend void dec_ref_count(StringData* s, void*)
    {
        if (s->refcount != -1 and --s->refcount == 0)
            StringData::destroy(s);
    }
};

using StringDataPtr = RefPtr<StringData>;

inline StringDataPtr operator"" _ss(const char* ptr, size_t len)
{
    return StringData::create({ptr, (int)len});
}

template<size_t len>
struct StaticStringData : StringData
{
    template<size_t... I> constexpr
    StaticStringData(const char (&literal)[len], IndexSequence<I...>)
        : StringData{-1, len}, data{literal[I]...} {}

    const char data[len];
};

template<size_t len>
constexpr StaticStringData<len> static_storage(const char (&literal)[len])
{
    return { literal, make_index_sequence<len>() };
}

class SharedString : public StringView
{
public:
    SharedString() = default;
    SharedString(StringView str)
    {
        if (not str.empty())
        {
            m_storage = StringData::create(str);
            StringView::operator=(m_storage->strview());
        }
    }
    struct NoCopy{};
    SharedString(StringView str, NoCopy) : StringView(str) {}

    SharedString(const char* str) : SharedString(StringView{str}) {}

    SharedString acquire_substr(ByteCount from, ByteCount length = INT_MAX) const
    {
        return SharedString{StringView::substr(from, length), m_storage};
    }
    SharedString acquire_substr(CharCount from, CharCount length = INT_MAX) const
    {
        return SharedString{StringView::substr(from, length), m_storage};
    }

    explicit SharedString(StringDataPtr storage)
        : StringView{storage->strview()}, m_storage(std::move(storage)) {}

private:
    SharedString(StringView str, StringDataPtr storage)
        : StringView{str}, m_storage(std::move(storage)) {}

    friend class StringRegistry;
    StringDataPtr m_storage;
};

inline size_t hash_value(const SharedString& str)
{
    return hash_data(str.data(), (int)str.length());
}

class StringRegistry : public Singleton<StringRegistry>
{
public:
    void debug_stats() const;
    SharedString intern(StringView str);
    void purge_unused();

private:
    UnorderedMap<StringView, StringDataPtr, MemoryDomain::SharedString> m_strings;
};

inline SharedString intern(StringView str)
{
    return StringRegistry::instance().intern(str);
}

}

#endif // shared_string_hh_INCLUDED
