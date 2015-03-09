#ifndef array_view_hh_INCLUDED
#define array_view_hh_INCLUDED

#include <vector>
#include <initializer_list>

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

    constexpr ArrayView(const T& oneval)
        : m_pointer(&oneval), m_size(1) {}

    constexpr ArrayView(const T* pointer, size_t size)
        : m_pointer(pointer), m_size(size) {}

    constexpr ArrayView(const T* begin, const T* end)
        : m_pointer(begin), m_size(end - begin) {}

    template<size_t N>
    constexpr ArrayView(const T(&array)[N]) : m_pointer(array), m_size(N) {}

    template<typename Iterator>
    constexpr ArrayView(const Iterator& begin, const Iterator& end)
        : m_pointer(&(*begin)), m_size(end - begin) {}

    template<typename Alloc>
    constexpr ArrayView(const std::vector<T, Alloc>& v)
        : m_pointer(&v[0]), m_size(v.size()) {}

    constexpr ArrayView(const std::initializer_list<T>& v)
        : m_pointer(v.begin()), m_size(v.size()) {}

    constexpr const T* pointer() const { return m_pointer; }
    constexpr size_t size() const { return m_size; }
    constexpr const T& operator[](size_t n) const { return *(m_pointer + n); }

    constexpr const T* begin() const { return m_pointer; }
    constexpr const T* end()   const { return m_pointer+m_size; }

    using reverse_iterator = std::reverse_iterator<const T*>;
    constexpr reverse_iterator rbegin() const { return reverse_iterator(m_pointer+m_size); }
    constexpr reverse_iterator rend()   const { return reverse_iterator(m_pointer); }

    constexpr const T& front() const { return *m_pointer; }
    constexpr const T& back()  const { return *(m_pointer + m_size - 1); }

    constexpr bool empty() const { return m_size == 0; }

    constexpr ArrayView subrange(size_t first, size_t count) const
    {
        return ArrayView(m_pointer + first, count);
    }

private:
    const T* m_pointer;
    size_t   m_size;
};

}

#endif // array_view_hh_INCLUDED
