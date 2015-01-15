#ifndef shared_string_hh_INCLUDED
#define shared_string_hh_INCLUDED

#include "string.hh"
#include "utils.hh"
#include "unordered_map.hh"

#include <memory>

namespace Kakoune
{

class SharedString : public StringView
{
public:
    using Storage = std::basic_string<char, std::char_traits<char>,
                                      Allocator<char, MemoryDomain::SharedString>>;
    SharedString() = default;
    SharedString(StringView str)
    {
        if (not str.empty())
        {
            m_storage = std::make_shared<Storage>(str.begin(), str.end());
            StringView::operator=(*m_storage);
        }
    }
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
    SharedString(StringView str, std::shared_ptr<Storage> storage)
        : StringView{str}, m_storage(std::move(storage)) {}

    friend class StringRegistry;
    std::shared_ptr<Storage> m_storage;
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
    UnorderedMap<StringView, std::shared_ptr<SharedString::Storage>, MemoryDomain::SharedString> m_strings;
};

inline SharedString intern(StringView str)
{
    return StringRegistry::instance().intern(str);
}

}

#endif // shared_string_hh_INCLUDED
