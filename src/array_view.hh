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

    ArrayView()
        : m_pointer(nullptr), m_size(0) {}

    ArrayView(const T& oneval)
        : m_pointer(&oneval), m_size(1) {}

    ArrayView(const T* pointer, size_t size)
        : m_pointer(pointer), m_size(size) {}

    ArrayView(const T* begin, const T* end)
        : m_pointer(begin), m_size(end - begin) {}

    template<size_t N>
    ArrayView(const T(&array)[N]) : m_pointer(array), m_size(N) {}

    template<typename Iterator>
    ArrayView(const Iterator& begin, const Iterator& end)
        : m_pointer(&(*begin)), m_size(end - begin) {}

    template<typename Alloc>
    ArrayView(const std::vector<T, Alloc>& v)
        : m_pointer(&v[0]), m_size(v.size()) {}

    ArrayView(const std::initializer_list<T>& v)
        : m_pointer(v.begin()), m_size(v.size()) {}

    const T* pointer() const { return m_pointer; }
    size_t size() const { return m_size; }
    const T& operator[](size_t n) const { return *(m_pointer + n); }

    const T* begin() const { return m_pointer; }
    const T* end()   const { return m_pointer+m_size; }

    using reverse_iterator = std::reverse_iterator<const T*>;
    reverse_iterator rbegin() const { return reverse_iterator(m_pointer+m_size); }
    reverse_iterator rend()   const { return reverse_iterator(m_pointer); }

    const T& front() const { return *m_pointer; }
    const T& back()  const { return *(m_pointer + m_size - 1); }

    bool empty() const { return m_size == 0; }

    ArrayView subrange(size_t first, size_t count) const
    {
        return ArrayView(m_pointer + first, count);
    }

private:
    const T* m_pointer;
    size_t   m_size;
};

}

#endif // array_view_hh_INCLUDED
