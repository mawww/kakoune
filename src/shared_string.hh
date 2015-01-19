#ifndef shared_string_hh_INCLUDED
#define shared_string_hh_INCLUDED

#include "string.hh"
#include "ref_ptr.hh"
#include "utils.hh"
#include "unordered_map.hh"

namespace Kakoune
{

class SharedString : public StringView
{
public:
    struct Storage : UseMemoryDomain<MemoryDomain::SharedString>
    {
        int refcount;
        int length;
        char data[1];

        StringView strview() const { return {data, length}; }

        static Storage* create(StringView str)
        {
            const int len = (int)str.length();
            void* ptr = Storage::operator new(sizeof(Storage) + len);
            Storage* res = reinterpret_cast<Storage*>(ptr);
            memcpy(res->data, str.data(), len);
            res->refcount = 0;
            res->length = len;
            res->data[len] = 0;
            return res;
        }

        static void destroy(Storage* s)
        {
            Storage::operator delete(s, sizeof(Storage) + s->length);
        }

        friend void inc_ref_count(Storage* s) { ++s->refcount; }
        friend void dec_ref_count(Storage* s) { if (--s->refcount == 0) Storage::destroy(s); }
    };

    SharedString() = default;
    SharedString(StringView str)
    {
        if (not str.empty())
        {
            m_storage = Storage::create(str);
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

private:
    SharedString(StringView str, ref_ptr<Storage> storage)
        : StringView{str}, m_storage(std::move(storage)) {}

    friend class StringRegistry;
    ref_ptr<Storage> m_storage;
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
    UnorderedMap<StringView, ref_ptr<SharedString::Storage>, MemoryDomain::SharedString> m_strings;
};

inline SharedString intern(StringView str)
{
    return StringRegistry::instance().intern(str);
}

}

#endif // shared_string_hh_INCLUDED
