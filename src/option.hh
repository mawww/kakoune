#ifndef option_hh_INCLUDED
#define option_hh_INCLUDED

#include "enum.hh"
#include "meta.hh"
#include "vector.hh"
#include "constexpr_utils.hh"

namespace Kakoune
{

class String;
enum class Quoting;

// Forward declare functions that wont get found by ADL
inline String option_to_string(int opt);
inline String option_to_string(size_t opt);
inline String option_to_string(bool opt);

// Default fallback to single value functions
template<typename T>
decltype(option_from_string(Meta::Type<T>{}, StringView{}))
option_from_strings(Meta::Type<T>, ConstArrayView<String> strs)
{
    if (strs.size() != 1)
        throw runtime_error("expected a single value for option");
    return option_from_string(Meta::Type<T>{}, strs[0]);
}

template<typename T>
Vector<decltype(option_to_string(std::declval<T>(), Quoting{}))>
option_to_strings(const T& opt)
{
    return Vector<String>{option_to_string(opt, Quoting{})};
}

template<typename T>
decltype(option_add(std::declval<T>(), std::declval<String>()))
option_add_from_strings(T& opt, ConstArrayView<String> strs)
{
    if (strs.size() != 1)
        throw runtime_error("expected a single value for option");
    return option_add(opt, strs[0]);
}

template<typename T>
decltype(option_add(std::declval<T>(), std::declval<String>()))
option_remove_from_strings(T& opt, ConstArrayView<String> strs)
{
    if (strs.size() != 1)
        throw runtime_error("expected a single value for option");
    return option_remove(opt, strs[0]);
}

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
    None     = 0,
    Hooks    = 1 << 0,
    Shell    = 1 << 1,
    Profile  = 1 << 2,
    Keys     = 1 << 3,
    Commands = 1 << 4,
};

constexpr bool with_bit_ops(Meta::Type<DebugFlags>) { return true; }

constexpr auto enum_desc(Meta::Type<DebugFlags>)
{
    return make_array<EnumDesc<DebugFlags>>({
        { DebugFlags::Hooks, "hooks" },
        { DebugFlags::Shell, "shell" },
        { DebugFlags::Profile, "profile" },
        { DebugFlags::Keys, "keys" },
        { DebugFlags::Commands, "commands" },
    });
}

}

#endif // option_hh_INCLUDED
