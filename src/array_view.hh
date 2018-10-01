#pragma once

#include <vector>
#include <initializer_list>
#include <iterator>

namespace Kakoune
{

// An ArrayView provides a typed, non owning view of a memory
// range with an interface similar to std::vector.
template<typename T>
class ArrayView
{
public:
    using size_t = std::size_t;

    constexpr ArrayView()
        : m_pointer(nullptr), m_size(0) {}

    constexpr ArrayView(T& oneval)
        : m_pointer(&oneval), m_size(1) {}

    constexpr ArrayView(T* pointer, size_t size)
        : m_pointer(pointer), m_size(size) {}

    constexpr ArrayView(T* begin, T* end)
        : m_pointer(begin), m_size(end - begin) {}

    template<size_t N>
    constexpr ArrayView(T(&array)[N]) : m_pointer(array), m_size(N) {}

    template<typename Alloc, typename U,
             typename = std::enable_if_t<sizeof(U) == sizeof(T)>>
    constexpr ArrayView(const std::vector<U, Alloc>& v)
        : m_pointer(v.data()), m_size(v.size()) {}

    constexpr ArrayView(const std::initializer_list<T>& v)
        : m_pointer(v.begin()), m_size(v.size()) {}

    constexpr T* pointer() const { return m_pointer; }
    constexpr size_t size() const { return m_size; }

    [[gnu::always_inline]]
    constexpr T& operator[](size_t n) const { return *(m_pointer + n); }

    constexpr T* begin() const { return m_pointer; }
    constexpr T* end()   const { return m_pointer+m_size; }

    using reverse_iterator = std::reverse_iterator<T*>;
    constexpr reverse_iterator rbegin() const { return reverse_iterator(m_pointer+m_size); }
    constexpr reverse_iterator rend()   const { return reverse_iterator(m_pointer); }

    constexpr T& front() const { return *m_pointer; }
    constexpr T& back()  const { return *(m_pointer + m_size - 1); }

    constexpr bool empty() const { return m_size == 0; }

    constexpr ArrayView subrange(size_t first, size_t count = -1) const
    {
        return ArrayView(m_pointer + std::min(first, m_size),
                         std::min(count, m_size - std::min(first, m_size)));
    }

private:
    T* m_pointer;
    size_t m_size;
};

template<typename T>
using ConstArrayView = ArrayView<const T>;

template<typename T>
bool operator==(ArrayView<T> lhs, ArrayView<T> rhs)
{
    if (lhs.size() != rhs.size())
        return false;
    for (int i = 0; i < lhs.size(); ++i)
    {
        if (lhs[i] != rhs[i])
            return false;
    }
    return true;
}

template<typename T>
bool operator!=(ArrayView<T> lhs, ArrayView<T> rhs)
{
    return not (lhs == rhs);
}

}
