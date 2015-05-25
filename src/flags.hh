#ifndef flags_hh_INCLUDED
#define flags_hh_INCLUDED

#include <type_traits>

namespace Kakoune
{

template<typename Flags>
struct WithBitOps : std::false_type {};

template<typename Flags>
using EnableIfWithBitOps = std::enable_if_t<WithBitOps<Flags>::value>;

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
constexpr Flags operator|(Flags lhs, Flags rhs)
{
    return (Flags)((std::underlying_type_t<Flags>) lhs | (std::underlying_type_t<Flags>) rhs);
}

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
Flags& operator|=(Flags& lhs, Flags rhs)
{
    (std::underlying_type_t<Flags>&) lhs |= (std::underlying_type_t<Flags>) rhs;
    return lhs;
}

template<typename Flags>
struct TestableFlags
{
    Flags value;
    constexpr operator bool() const { return (std::underlying_type_t<Flags>)value; }
    constexpr operator Flags() const { return value; }
};

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
constexpr TestableFlags<Flags> operator&(Flags lhs, Flags rhs)
{
    return { (Flags)((std::underlying_type_t<Flags>) lhs & (std::underlying_type_t<Flags>) rhs) };
}

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
Flags& operator&=(Flags& lhs, Flags rhs)
{
    (std::underlying_type_t<Flags>&) lhs &= (std::underlying_type_t<Flags>) rhs;
    return lhs;
}

template<typename Flags, typename = EnableIfWithBitOps<Flags>>
constexpr Flags operator~(Flags lhs)
{
    return (Flags)(~(std::underlying_type_t<Flags>)lhs);
}

}

#endif // flags_hh_INCLUDED
