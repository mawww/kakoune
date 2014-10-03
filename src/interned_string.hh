#ifndef interned_string_hh_INCLUDED
#define interned_string_hh_INCLUDED

#include "string.hh"
#include "utils.hh"

#include <unordered_map>

namespace Kakoune
{

class InternedString;

class StringRegistry : public Singleton<StringRegistry>
{
private:
    friend class InternedString;

    InternedString acquire(StringView str);
    void release(StringView str);

    std::unordered_map<StringView, size_t> m_slot_map;
    std::vector<size_t> m_free_slots;
    using DataAndRefCount = std::pair<std::vector<char>, int>;
    std::vector<DataAndRefCount> m_storage;
};

class InternedString : public StringView
{
public:
    InternedString() = default;

    InternedString(const InternedString& str) { acquire_ifn(str); }

    InternedString(InternedString&& str) : StringView(str)
    {
        static_cast<StringView&>(str) = StringView{};
    }

    InternedString(const char* str) : StringView() { acquire_ifn(str); }
    InternedString(StringView str) : StringView() { acquire_ifn(str); }
    //InternedString(const String& str) : StringView() { acquire_ifn(str); }

    InternedString& operator=(const InternedString& str)
    {
        if (str.data() == data() && str.length() == length())
            return *this;
        release_ifn();
        acquire_ifn(str);
        return *this;
    }

    InternedString& operator=(InternedString&& str)
    {
        static_cast<StringView&>(*this) = str;
        static_cast<StringView&>(str) = StringView{};
        return *this;
    }

    ~InternedString()
    {
        release_ifn();
    }

    bool operator==(const InternedString& str) const
    { return data() == str.data() && length() == str.length(); }
    bool operator!=(const InternedString& str) const
    { return !(*this == str); }

    using StringView::operator==;
    using StringView::operator!=;

private:
    friend class StringRegistry;

    struct AlreadyAcquired{};
    InternedString(StringView str, AlreadyAcquired)
        : StringView(str) {}

    void acquire_ifn(StringView str)
    {
        if (str.empty())
            static_cast<StringView&>(*this) = StringView{};
        else
            *this = StringRegistry::instance().acquire(str);
    }

    void release_ifn()
    {
        if (!empty())
            StringRegistry::instance().release(*this);
    }
};

}

namespace std
{
    template<>
    struct hash<Kakoune::InternedString>
    {
        size_t operator()(const Kakoune::InternedString& str) const
        {
            return hash<const char*>{}(str.data()) ^
                   hash<int>{}((int)str.length());
        }
    };
}

#endif // interned_string_hh_INCLUDED

