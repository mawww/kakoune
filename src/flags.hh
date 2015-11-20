#ifndef flags_hh_INCLUDED
#define flags_hh_INCLUDED

#include <type_traits>

namespace Kakoune
{

template<typename Flags>
struct WithBitOps : std::false_type {};

template<typename Flags>
using UnderlyingType = typename std::underlying_type<Flags>::type;

template<typename Flags, typename T = void>
using EnableIfWithBitOps = typename std::enable_if<WithBitOps<Flags>::value, T>::type;

template<typename Flags, typename T = void>
using EnableIfWithoutBitOps = typename std::enable_if<not WithBitOps<Flags>::value, T>::type;

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
constexpr Flags operator|(Flags lhs, Flags rhs)
{
    return (Flags)((UnderlyingType<Flags>) lhs | (UnderlyingType<Flags>) rhs);
}

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
Flags& operator|=(Flags& lhs, Flags rhs)
{
    (UnderlyingType<Flags>&) lhs |= (UnderlyingType<Flags>) rhs;
    return lhs;
}

template<typename Flags>
struct TestableFlags
{
    Flags value;
    constexpr operator bool() const { return (UnderlyingType<Flags>)value; }
    constexpr operator Flags() const { return value; }
};

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
constexpr TestableFlags<Flags> operator&(Flags lhs, Flags rhs)
{
    return { (Flags)((UnderlyingType<Flags>) lhs & (UnderlyingType<Flags>) rhs) };
}

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
Flags& operator&=(Flags& lhs, Flags rhs)
{
    (UnderlyingType<Flags>&) lhs &= (UnderlyingType<Flags>) rhs;
    return lhs;
}

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
constexpr Flags operator~(Flags lhs)
{
    return (Flags)(~(UnderlyingType<Flags>)lhs);
}

}

#endif // flags_hh_INCLUDED
