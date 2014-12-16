#ifndef hash_hh_INCLUDED
#define hash_hh_INCLUDED

#include <type_traits>
#include <functional>

namespace Kakoune
{

size_t hash_data(const char* data, size_t len);

template<typename Type>
typename std::enable_if<not std::is_enum<Type>::value, size_t>::type
hash_value(const Type& val)
{
    return std::hash<Type>()(val);
}

template<typename Type>
typename std::enable_if<std::is_enum<Type>::value, size_t>::type
hash_value(const Type& val)
{
    return hash_value((typename std::underlying_type<Type>::type)val);
}

template<typename Type>
size_t hash_values(Type&& t)
{
    return hash_value(std::forward<Type>(t));
}

template<typename Type, typename... RemainingTypes>
size_t hash_values(Type&& t, RemainingTypes&&... rt)
{
    size_t seed = hash_values(std::forward<RemainingTypes>(rt)...);
    return seed ^ (hash_value(std::forward<Type>(t)) + 0x9e3779b9 +
                   (seed << 6) + (seed >> 2));
}

template<typename T1, typename T2>
size_t hash_value(const std::pair<T1, T2>& val)
{
    return hash_values(val.first, val.second);
}

template<typename Type>
struct Hash
{
    size_t operator()(const Type& val) const
    {
        return hash_value(val);
    }
};

}

#endif // hash_hh_INCLUDED
