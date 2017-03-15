#ifndef enum_hh_INCLUDED
#define enum_hh_INCLUDED

#include "flags.hh"
#include "string.hh"
#include "exception.hh"
#include "containers.hh"
#include "meta.hh"

namespace Kakoune
{

template<typename T, size_t N>
struct Array
{
    constexpr size_t size() const { return N; }
    constexpr const T& operator[](int i) const { return m_data[i]; }
    constexpr const T* begin() const { return m_data; }
    constexpr const T* end() const { return m_data+N; }

    T m_data[N];
};

template<typename T> struct EnumDesc { T value; StringView name; };

template<typename Flags, typename = decltype(enum_desc(Meta::Type<Flags>{}))>
EnableIfWithBitOps<Flags, String> option_to_string(Flags flags)
{
    constexpr auto desc = enum_desc(Meta::Type<Flags>{});
    String res;
    for (int i = 0; i < desc.size(); ++i)
    {
        if (not (flags & desc[i].value))
            continue;
        if (not res.empty())
            res += "|";
        res += desc[i].name;
    }
    return res;
}

template<typename Enum, typename = decltype(enum_desc(Meta::Type<Enum>{}))>
EnableIfWithoutBitOps<Enum, String> option_to_string(Enum e)
{
    constexpr auto desc = enum_desc(Meta::Type<Enum>{});
    auto it = find_if(desc, [e](const EnumDesc<Enum>& d) { return d.value == e; });
    if (it != desc.end())
        return it->name.str();
    kak_assert(false);
    return {};
}

template<typename Flags, typename = decltype(enum_desc(Meta::Type<Flags>{}))>
EnableIfWithBitOps<Flags> option_from_string(StringView str, Flags& flags)
{
    constexpr auto desc = enum_desc(Meta::Type<Flags>{});
    flags = Flags{};
    for (auto s : str | split<StringView>('|'))
    {
        auto it = find_if(desc, [s](const EnumDesc<Flags>& d) { return d.name == s; });
        if (it == desc.end())
            throw runtime_error(format("invalid flag value '{}'", s));
        flags |= it->value;
    }
}

template<typename Enum, typename = decltype(enum_desc(Meta::Type<Enum>{}))>
EnableIfWithoutBitOps<Enum> option_from_string(StringView str, Enum& e)
{
    constexpr auto desc = enum_desc(Meta::Type<Enum>{});
    auto it = find_if(desc, [str](const EnumDesc<Enum>& d) { return d.name == str; });
    if (it == desc.end())
        throw runtime_error(format("invalid enum value '{}'", str));
    e = it->value;
}

template<typename Flags, typename = decltype(enum_desc(Meta::Type<Flags>{}))>
EnableIfWithBitOps<Flags, bool> option_add(Flags& opt, StringView str)
{
    Flags res = Flags{};
    option_from_string(str, res);
    opt |= res;
    return res != (Flags)0;
}

}

#endif // enum_hh_INCLUDED
