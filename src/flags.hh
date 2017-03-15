#ifndef flags_hh_INCLUDED
#define flags_hh_INCLUDED

#include <type_traits>

#include "meta.hh"

namespace Kakoune
{

template<typename Flags>
constexpr bool with_bit_ops(Meta::Type<Flags>) { return false; }

template<typename Flags>
using UnderlyingType = typename std::underlying_type<Flags>::type;

template<typename Flags, typename T = void>
using EnableIfWithBitOps = typename std::enable_if<with_bit_ops(Meta::Type<Flags>{}), T>::type;

template<typename Flags, typename T = void>
using EnableIfWithoutBitOps = typename std::enable_if<not with_bit_ops(Meta::Type<Flags>{}), T>::type;

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
    constexpr operator UnderlyingType<Flags>() const { return (UnderlyingType<Flags>)value; }

    bool operator==(const TestableFlags<Flags>& other) const { return value == other.value; }
    bool operator!=(const TestableFlags<Flags>& other) const { return value != other.value; }
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

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
constexpr Flags operator^(Flags lhs, Flags rhs)
{
    return (Flags)((UnderlyingType<Flags>) lhs ^ (UnderlyingType<Flags>) rhs);
}

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
Flags& operator^=(Flags& lhs, Flags rhs)
{
    (UnderlyingType<Flags>&) lhs ^= (UnderlyingType<Flags>) rhs;
    return lhs;
}

}

#endif // flags_hh_INCLUDED
