#ifndef option_hh_INCLUDED
#define option_hh_INCLUDED

#include "enum.hh"
#include "meta.hh"

namespace Kakoune
{

class String;

// Forward declare functions that wont get found by ADL
inline String option_to_string(int opt);
inline String option_to_string(size_t opt);
inline String option_to_string(bool opt);

template<typename P, typename T>
struct PrefixedList
{
    P prefix;
    Vector<T, MemoryDomain::Options> list;

    friend bool operator==(const PrefixedList& lhs, const PrefixedList& rhs)
    {
        return lhs.prefix == rhs.prefix and lhs.list == rhs.list;
    }

    friend bool operator!=(const PrefixedList& lhs, const PrefixedList& rhs)
    {
        return not (lhs == rhs);
    }
};

template<typename T>
using TimestampedList = PrefixedList<size_t, T>;

enum class DebugFlags
{
    None  = 0,
    Hooks = 1 << 0,
    Shell = 1 << 1,
    Profile = 1 << 2,
    Keys = 1 << 3,
};

constexpr bool with_bit_ops(Meta::Type<DebugFlags>) { return true; }

constexpr Array<EnumDesc<DebugFlags>, 4> enum_desc(Meta::Type<DebugFlags>)
{
    return { {
        { DebugFlags::Hooks, "hooks" },
        { DebugFlags::Shell, "shell" },
        { DebugFlags::Profile, "profile" },
        { DebugFlags::Keys, "keys" }
    } };
}

}

#endif // option_hh_INCLUDED
