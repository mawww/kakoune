#ifndef constexpr_utils_hh_INCLUDED
#define constexpr_utils_hh_INCLUDED

#include <utility>
#include <initializer_list>
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

template<typename T, size_t capacity>
struct ConstexprVector
{
    using iterator = T*;
    using const_iterator = const T*;

    constexpr ConstexprVector() : m_size{0} {}
    constexpr ConstexprVector(std::initializer_list<T> items)
        : m_size{items.size()}
    {
        T* ptr = m_data;
        for (auto& item : items)
            *ptr++ = std::move(item);
    }

    constexpr bool empty() const { return m_size == 0; }
    constexpr size_t size() const { return m_size; }

    constexpr void resize(size_t n, const T& val = {})
    {
        if (n >= capacity)
            throw "capacity exceeded";
        for (int i = m_size; i < n; ++i)
            m_data[i] = val;
        m_size = n;
        kak_assert(this->size() == m_size); // check for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=79520
    }

    constexpr T& operator[](size_t i) { return m_data[i]; }
    constexpr const T& operator[](size_t i) const { return m_data[i]; }

    constexpr iterator begin() { return m_data; }
    constexpr iterator end() { return m_data + m_size; }

    constexpr const_iterator begin() const { return m_data; }
    constexpr const_iterator end() const { return m_data + m_size; }

    size_t m_size;
    T m_data[capacity] = {};
};

}

#endif // constexpr_utils_hh_INCLUDED
