#ifndef units_hh_INCLUDED
#define units_hh_INCLUDED

#include <type_traits>

namespace Kakoune
{

template<typename RealType, typename ValueType = int>
class StronglyTypedNumber
{
public:
    explicit constexpr StronglyTypedNumber(ValueType value)
        : m_value(value)
    {
        static_assert(std::is_base_of<StronglyTypedNumber, RealType>::value,
                     "RealType is not derived from StronglyTypedNumber");
    }

    constexpr RealType operator+(RealType other) const
    { return RealType(m_value + other.m_value); }

    constexpr RealType operator-(RealType other) const
    { return RealType(m_value - other.m_value); }

    constexpr RealType operator*(RealType other) const
    { return RealType(m_value * other.m_value); }

    constexpr RealType operator/(RealType other) const
    { return RealType(m_value / other.m_value); }

    RealType& operator+=(RealType other)
    { m_value += other.m_value; return static_cast<RealType&>(*this); }

    RealType& operator-=(RealType other)
    { m_value -= other.m_value; return static_cast<RealType&>(*this); }

    RealType& operator*=(RealType other)
    { m_value *= other.m_value; return static_cast<RealType&>(*this); }

    RealType& operator/=(RealType other)
    { m_value /= other.m_value; return static_cast<RealType&>(*this); }

    RealType& operator++()
    { ++m_value; return static_cast<RealType&>(*this); }

    RealType& operator--()
    { --m_value; return static_cast<RealType&>(*this); }

    RealType operator++(int)
    { RealType backup(static_cast<RealType&>(*this)); ++m_value; return backup; }

    RealType operator--(int)
    { RealType backup(static_cast<RealType&>(*this)); --m_value; return backup; }

    constexpr RealType operator-() const { return RealType(-m_value); }

    constexpr RealType operator%(RealType other) const
    { return RealType(m_value % other.m_value); }

    RealType& operator%=(RealType other)
    { m_value %= other.m_value; return static_cast<RealType&>(*this); }

    constexpr bool operator==(RealType other) const
    { return m_value == other.m_value; }

    constexpr bool operator!=(RealType other) const
    { return m_value != other.m_value; }

    constexpr bool operator<(RealType other) const
    { return m_value < other.m_value; }

    constexpr bool operator<=(RealType other) const
    { return m_value <= other.m_value; }

    constexpr bool operator>(RealType other) const
    { return m_value > other.m_value; }

    constexpr bool operator>=(RealType other) const
    { return m_value >= other.m_value; }

    constexpr bool operator!() const
    { return !m_value; }

    explicit constexpr operator ValueType() const { return m_value; }
    explicit constexpr operator bool() const { return m_value; }
private:
    ValueType m_value;
};

struct LineCount : public StronglyTypedNumber<LineCount, int>
{
    constexpr LineCount(int value = 0) : StronglyTypedNumber<LineCount>(value) {}
};

inline constexpr LineCount operator"" _line(unsigned long long int value)
{
    return LineCount(value);
}

struct ByteCount : public StronglyTypedNumber<ByteCount, int>
{
    constexpr ByteCount(int value = 0) : StronglyTypedNumber<ByteCount>(value) {}
};

inline constexpr ByteCount operator"" _byte(unsigned long long int value)
{
    return ByteCount(value);
}

struct CharCount : public StronglyTypedNumber<CharCount, int>
{
    constexpr CharCount(int value = 0) : StronglyTypedNumber<CharCount>(value) {}
};

inline constexpr CharCount operator"" _char(unsigned long long int value)
{
    return CharCount(value);
}

}

#endif // units_hh_INCLUDED
