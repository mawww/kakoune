#ifndef hash_hh_INCLUDED
#define hash_hh_INCLUDED

#include <type_traits>
#include <functional>

#include <cstddef>

namespace Kakoune
{

size_t hash_data(const char* data, size_t len);

template<typename... Type>
constexpr size_t hash_value(const Type&... val)
{
    static_assert(sizeof...(Type) == 1, "");
    return std::hash<Type...>()(val...);
}

template<typename Type>
std::enable_if_t<std::is_integral<Type>::value, size_t> constexpr hash_value(
    const Type& val)
{
    return (size_t)val;
}

template<typename Type>
std::enable_if_t<std::is_enum<Type>::value, size_t> constexpr hash_value(
    const Type& val)
{
    return hash_value((std::underlying_type_t<Type>)val);
}

template<typename Type>
constexpr size_t hash_values(Type&& t)
{
    return hash_value(std::forward<Type>(t));
}

constexpr size_t combine_hash(size_t lhs, size_t rhs)
{
    return lhs ^ (rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2));
}

template<typename Type, typename... RemainingTypes>
constexpr size_t hash_values(Type&& t, RemainingTypes&&... rt)
{
    size_t seed = hash_values(std::forward<RemainingTypes>(rt)...);
    return combine_hash(seed, hash_value(std::forward<Type>(t)));
}

template<typename T1, typename T2>
constexpr size_t hash_value(const std::pair<T1, T2>& val)
{
    return hash_values(val.first, val.second);
}

template<typename Type>
struct Hash
{
    constexpr size_t operator()(const Type& val) const
    {
        return hash_value(val);
    }
};

// Traits specifying if two types have compatible hashing, that is,
// if lhs == rhs => hash_value(lhs) == hash_value(rhs)
template<typename Lhs, typename Rhs>
struct HashCompatible : std::false_type
{};

template<typename T>
struct HashCompatible<T, T> : std::true_type
{};

template<typename Lhs, typename Rhs>
constexpr bool IsHashCompatible = HashCompatible<Lhs, Rhs>::value;

}

#endif // hash_hh_INCLUDED
