#ifndef array_hh_INCLUDED
#define array_hh_INCLUDED

#include <utility>
#include <stddef.h>

#include "array_view.hh"

namespace Kakoune
{

template<typename T, size_t N>
struct Array
{
    constexpr size_t size() const { return N; }
    constexpr const T& operator[](int i) const { return m_data[i]; }
    constexpr const T* begin() const { return m_data; }
    constexpr const T* end() const { return m_data+N; }

    constexpr T& operator[](int i) { return m_data[i]; }
    constexpr T* begin() { return m_data; }
    constexpr T* end() { return m_data+N; }

    constexpr operator ArrayView<T>() { return {m_data, N}; }
    constexpr operator ConstArrayView<T>() const { return {m_data, N}; }

    T m_data[N];
};

template<typename T, typename... U> requires (std::is_same_v<T, U> and ...)
Array(T, U...) -> Array<T, 1 + sizeof...(U)>;

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

#endif // array_hh_INCLUDED
