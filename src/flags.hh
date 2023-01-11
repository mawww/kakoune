#ifndef flags_hh_INCLUDED
#define flags_hh_INCLUDED

#include <type_traits>

#include "meta.hh"

namespace Kakoune
{

template<typename Flags>
constexpr bool with_bit_ops(Meta::Type<Flags>) { return false; }

template<typename Flags>
concept WithBitOps = with_bit_ops(Meta::Type<Flags>{});

template<typename Flags>
using UnderlyingType = std::underlying_type_t<Flags>;

template<WithBitOps Flags>
constexpr Flags operator|(Flags lhs, Flags rhs)
{
    return (Flags)((UnderlyingType<Flags>) lhs | (UnderlyingType<Flags>) rhs);
}

template<WithBitOps Flags>
constexpr Flags& operator|=(Flags& lhs, Flags rhs)
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

    constexpr bool operator==(const TestableFlags<Flags>& other) const { return value == other.value; }
    constexpr bool operator!=(const TestableFlags<Flags>& other) const { return value != other.value; }
};

template<WithBitOps Flags>
constexpr TestableFlags<Flags> operator&(Flags lhs, Flags rhs)
{
    return { (Flags)((UnderlyingType<Flags>) lhs & (UnderlyingType<Flags>) rhs) };
}

template<WithBitOps Flags>
constexpr Flags& operator&=(Flags& lhs, Flags rhs)
{
    (UnderlyingType<Flags>&) lhs &= (UnderlyingType<Flags>) rhs;
    return lhs;
}

template<WithBitOps Flags>
constexpr Flags operator~(Flags lhs)
{
    return (Flags)(~(UnderlyingType<Flags>)lhs);
}

template<WithBitOps Flags>
constexpr Flags operator^(Flags lhs, Flags rhs)
{
    return (Flags)((UnderlyingType<Flags>) lhs ^ (UnderlyingType<Flags>) rhs);
}

template<WithBitOps Flags>
constexpr Flags& operator^=(Flags& lhs, Flags rhs)
{
    (UnderlyingType<Flags>&) lhs ^= (UnderlyingType<Flags>) rhs;
    return lhs;
}

}

#endif // flags_hh_INCLUDED
