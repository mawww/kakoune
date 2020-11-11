#ifndef array_view_hh_INCLUDED
#define array_view_hh_INCLUDED

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

    template<typename Container>
        requires (sizeof(decltype(*std::declval<Container>().data())) == sizeof(T))
    constexpr ArrayView(Container&& c)
        : m_pointer(c.data()), m_size(c.size()) {}

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
        auto min = [](size_t a, size_t b) { return a < b ? a : b; };
        return ArrayView(m_pointer + min(first, m_size),
                         min(count, m_size - min(first, m_size)));
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

#endif // array_view_hh_INCLUDED
