#ifndef flags_hh_INCLUDED
#define flags_hh_INCLUDED

#include <type_traits>

namespace Kakoune
{

template<typename Flags>
struct WithBitOps : std::false_type {};

template<typename Flags>
using EnumStorageType = typename std::underlying_type<Flags>::type;

template<typename Flags>
using EnableIfWithBitOps = typename std::enable_if<WithBitOps<Flags>::value>::type;

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
constexpr Flags operator|(Flags lhs, Flags rhs)
{
    return (Flags)((EnumStorageType<Flags>) lhs | (EnumStorageType<Flags>) rhs);
}

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
Flags& operator|=(Flags& lhs, Flags rhs)
{
    (EnumStorageType<Flags>&) lhs |= (EnumStorageType<Flags>) rhs;
    return lhs;
}

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
constexpr bool operator&(Flags lhs, Flags rhs)
{
    return ((EnumStorageType<Flags>) lhs & (EnumStorageType<Flags>) rhs) != 0;
}

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
Flags& operator&=(Flags& lhs, Flags rhs)
{
    (EnumStorageType<Flags>&) lhs &= (EnumStorageType<Flags>) rhs;
    return lhs;
}

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
constexpr Flags operator~(Flags lhs)
{
    return (Flags)(~(EnumStorageType<Flags>)lhs);
}

}

#endif // flags_hh_INCLUDED
