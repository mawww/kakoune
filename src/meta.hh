#ifndef meta_hh_INCLUDED
#define meta_hh_INCLUDED

#include <utility>
#include <stddef.h>

namespace Kakoune
{
inline namespace Meta
{

struct AnyType{};
template<typename T> struct Type : AnyType {};

}

template<typename T, size_t N>
struct Array
{
    constexpr size_t size() const { return N; }
    constexpr const T& operator[](int i) const { return m_data[i]; }
    constexpr const T* begin() const { return m_data; }
    constexpr const T* end() const { return m_data+N; }

    T m_data[N];
};

template<typename T, size_t N, size_t... Indices>
constexpr Array<T, N> make_array(const T (&data)[N], std::index_sequence<Indices...>)
{
    static_assert(sizeof...(Indices) == N, "size mismatch");
    return {{data[Indices]...}};
}

template<typename T, size_t N>
constexpr Array<T, N> make_array(const T (&data)[N])
{
    return make_array(data, std::make_index_sequence<N>());
}

}

#endif // meta_hh_INCLUDED
