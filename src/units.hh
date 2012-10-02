#ifndef units_hh_INCLUDED
#define units_hh_INCLUDED

namespace Kakoune
{

template<typename RealType, typename ValueType = int>
class StronglyTypedInteger
{
public:
    explicit constexpr StronglyTypedInteger(ValueType value)
         : m_value(value) {}

    constexpr RealType operator+(const RealType& other) const
    { return RealType(m_value + other.m_value); }

    constexpr RealType operator-(const RealType& other) const
    { return RealType(m_value - other.m_value); }

    constexpr RealType operator*(const RealType& other) const
    { return RealType(m_value * other.m_value); }

    constexpr RealType operator/(const RealType& other) const
    { return RealType(m_value / other.m_value); }

    RealType& operator+=(const RealType& other)
    { m_value += other.m_value; return static_cast<RealType&>(*this); }

    RealType& operator-=(const RealType& other)
    { m_value -= other.m_value; return static_cast<RealType&>(*this); }

    RealType& operator*=(const RealType& other)
    { m_value *= other.m_value; return static_cast<RealType&>(*this); }

    RealType& operator/=(const RealType& other)
    { m_value /= other.m_value; return static_cast<RealType&>(*this); }

    RealType& operator++()
    { ++m_value; return static_cast<RealType&>(*this); }

    RealType& operator--()
    { --m_value; return static_cast<RealType&>(*this); }

    RealType operator++(int)
    { RealType backup(static_cast<RealType&>(*this)); ++m_value; return backup; }

    RealType operator--(int)
    { RealType backup(static_cast<RealType&>(*this)); --m_value; return backup; }

    constexpr RealType operator-() { return RealType(-m_value); }

    constexpr bool operator==(const RealType& other) const
    { return m_value == other.m_value; }

    constexpr bool operator!=(const RealType& other) const
    { return m_value != other.m_value; }

    constexpr bool operator<(const RealType& other) const
    { return m_value < other.m_value; }

    constexpr bool operator<=(const RealType& other) const
    { return m_value <= other.m_value; }

    constexpr bool operator>(const RealType& other) const
    { return m_value > other.m_value; }

    constexpr bool operator>=(const RealType& other) const
    { return m_value >= other.m_value; }

    constexpr bool operator!() const
    { return !m_value; }

    explicit constexpr operator ValueType() const { return m_value; }
    explicit constexpr operator bool() const { return m_value; }
private:
   ValueType m_value;
};

struct LineCount : public StronglyTypedInteger<LineCount, int>
{
    constexpr LineCount(int value) : StronglyTypedInteger<LineCount>(value) {}
};

inline constexpr LineCount operator"" _line(unsigned long long int value)
{
    return LineCount(value);
}

struct CharCount : public StronglyTypedInteger<CharCount, int>
{
    constexpr CharCount(int value) : StronglyTypedInteger<CharCount>(value) {}
};

inline constexpr CharCount operator"" _char(unsigned long long int value)
{
    return CharCount(value);
}

}

#endif // units_hh_INCLUDED

