#ifndef shared_string_hh_INCLUDED
#define shared_string_hh_INCLUDED

#include "string.hh"
#include "ref_ptr.hh"
#include "utils.hh"
#include "unordered_map.hh"

namespace Kakoune
{

struct StringStorage : UseMemoryDomain<MemoryDomain::SharedString>
{
    int refcount;
    int length;
    char data[1];

    StringView strview() const { return {data, length}; }

    static StringStorage* create(StringView str)
    {
        const int len = (int)str.length();
        void* ptr = StringStorage::operator new(sizeof(StringStorage) + len);
        StringStorage* res = reinterpret_cast<StringStorage*>(ptr);
        memcpy(res->data, str.data(), len);
        res->refcount = 0;
        res->length = len;
        res->data[len] = 0;
        return res;
    }

    static void destroy(StringStorage* s)
    {
        StringStorage::operator delete(s, sizeof(StringStorage) + s->length);
    }

    friend void inc_ref_count(StringStorage* s) { ++s->refcount; }
    friend void dec_ref_count(StringStorage* s) { if (--s->refcount == 0) StringStorage::destroy(s); }
};

class SharedString : public StringView
{
public:
    SharedString() = default;
    SharedString(StringView str)
    {
        if (not str.empty())
        {
            m_storage = StringStorage::create(str);
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

    explicit SharedString(ref_ptr<StringStorage> storage)
        : StringView{storage->strview()}, m_storage(std::move(storage)) {}

private:
    SharedString(StringView str, ref_ptr<StringStorage> storage)
        : StringView{str}, m_storage(std::move(storage)) {}

    friend class StringRegistry;
    ref_ptr<StringStorage> m_storage;
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
    UnorderedMap<StringView, ref_ptr<StringStorage>, MemoryDomain::SharedString> m_strings;
};

inline SharedString intern(StringView str)
{
    return StringRegistry::instance().intern(str);
}

}

#endif // shared_string_hh_INCLUDED
