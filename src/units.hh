#ifndef units_hh_INCLUDED
#define units_hh_INCLUDED

#include "hash.hh"

#include <type_traits>

namespace Kakoune
{

template<typename RealType, typename ValueType = int>
class StronglyTypedNumber
{
public:
    [[gnu::always_inline]]
    explicit constexpr StronglyTypedNumber(ValueType value)
        : m_value(value)
    {
        static_assert(std::is_base_of<StronglyTypedNumber, RealType>::value,
                     "RealType is not derived from StronglyTypedNumber");
    }

    [[gnu::always_inline]]
    constexpr RealType operator+(RealType other) const
    { return RealType(m_value + other.m_value); }

    [[gnu::always_inline]]
    constexpr RealType operator-(RealType other) const
    { return RealType(m_value - other.m_value); }

    [[gnu::always_inline]]
    constexpr RealType operator*(RealType other) const
    { return RealType(m_value * other.m_value); }

    [[gnu::always_inline]]
    constexpr RealType operator/(RealType other) const
    { return RealType(m_value / other.m_value); }

    [[gnu::always_inline]]
    RealType& operator+=(RealType other)
    { m_value += other.m_value; return static_cast<RealType&>(*this); }

    [[gnu::always_inline]]
    RealType& operator-=(RealType other)
    { m_value -= other.m_value; return static_cast<RealType&>(*this); }

    [[gnu::always_inline]]
    RealType& operator*=(RealType other)
    { m_value *= other.m_value; return static_cast<RealType&>(*this); }

    [[gnu::always_inline]]
    RealType& operator/=(RealType other)
    { m_value /= other.m_value; return static_cast<RealType&>(*this); }

    [[gnu::always_inline]]
    RealType& operator++()
    { ++m_value; return static_cast<RealType&>(*this); }

    [[gnu::always_inline]]
    RealType& operator--()
    { --m_value; return static_cast<RealType&>(*this); }

    [[gnu::always_inline]]
    RealType operator++(int)
    { RealType backup(static_cast<RealType&>(*this)); ++m_value; return backup; }

    [[gnu::always_inline]]
    RealType operator--(int)
    { RealType backup(static_cast<RealType&>(*this)); --m_value; return backup; }

    [[gnu::always_inline]]
    constexpr RealType operator-() const { return RealType(-m_value); }

    [[gnu::always_inline]]
    constexpr RealType operator%(RealType other) const
    { return RealType(m_value % other.m_value); }

    [[gnu::always_inline]]
    RealType& operator%=(RealType other)
    { m_value %= other.m_value; return static_cast<RealType&>(*this); }

    [[gnu::always_inline]]
    constexpr bool operator==(RealType other) const
    { return m_value == other.m_value; }

    [[gnu::always_inline]]
    constexpr bool operator!=(RealType other) const
    { return m_value != other.m_value; }

    [[gnu::always_inline]]
    constexpr bool operator<(RealType other) const
    { return m_value < other.m_value; }

    [[gnu::always_inline]]
    constexpr bool operator<=(RealType other) const
    { return m_value <= other.m_value; }

    [[gnu::always_inline]]
    constexpr bool operator>(RealType other) const
    { return m_value > other.m_value; }

    [[gnu::always_inline]]
    constexpr bool operator>=(RealType other) const
    { return m_value >= other.m_value; }

    [[gnu::always_inline]]
    constexpr bool operator!() const
    { return !m_value; }

    [[gnu::always_inline]]
    explicit constexpr operator ValueType() const { return m_value; }
    [[gnu::always_inline]]
    explicit constexpr operator bool() const { return m_value; }

    friend size_t hash_value(RealType val) { return hash_value(val.m_value); }
    friend size_t abs(RealType val) { return val.m_value < ValueType(0) ? -val.m_value : val.m_value; }

private:
    ValueType m_value;
};

struct LineCount : public StronglyTypedNumber<LineCount, int>
{
    [[gnu::always_inline]]
    constexpr LineCount(int value = 0) : StronglyTypedNumber<LineCount>(value) {}
};

[[gnu::always_inline]]
inline constexpr LineCount operator"" _line(unsigned long long int value)
{
    return LineCount(value);
}

struct ByteCount : public StronglyTypedNumber<ByteCount, int>
{
    [[gnu::always_inline]]
    constexpr ByteCount(int value = 0) : StronglyTypedNumber<ByteCount>(value) {}
};

[[gnu::always_inline]]
inline constexpr ByteCount operator"" _byte(unsigned long long int value)
{
    return ByteCount(value);
}

struct CharCount : public StronglyTypedNumber<CharCount, int>
{
    [[gnu::always_inline]]
    constexpr CharCount(int value = 0) : StronglyTypedNumber<CharCount>(value) {}
};

[[gnu::always_inline]]
inline constexpr CharCount operator"" _char(unsigned long long int value)
{
    return CharCount(value);
}

}

#endif // units_hh_INCLUDED
