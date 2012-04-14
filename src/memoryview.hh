#ifndef memoryview_hh_INCLUDED
#define memoryview_hh_INCLUDED

#include <vector>
#include <initializer_list>

namespace Kakoune
{

// A memoryview provides a typed, non owning view of a memory
// range with an interface similar to std::vector.
template<typename T>
class memoryview
{
public:
    memoryview()
        : m_pointer(nullptr), m_size(0) {}

    memoryview(const T& oneval)
        : m_pointer(&oneval), m_size(1) {}

    memoryview(const T* pointer, size_t size)
        : m_pointer(pointer), m_size(size) {}

    memoryview(const T* begin, const T* end)
        : m_pointer(begin), m_size(end - begin) {}

    template<typename Iterator>
    memoryview(const Iterator& begin, const Iterator& end)
        : m_pointer(&(*begin)), m_size(end - begin) {}

    memoryview(const std::vector<T>& v)
        : m_pointer(&v[0]), m_size(v.size()) {}

    memoryview(const std::initializer_list<T>& v)
        : m_pointer(v.begin()), m_size(v.size()) {}

    const T* pointer() const { return m_pointer; }
    size_t size() const { return m_size; }
    const T& operator[](size_t n) const { return *(m_pointer + n); }

    const T* begin() const { return m_pointer; }
    const T* end()   const { return m_pointer+m_size; }

    const T& front() const { return *m_pointer; }
    const T& back()  const { return *(m_pointer + m_size - 1); }

    bool empty() const { return m_size == 0; }

    memoryview subrange(size_t first, size_t count) const
    {
        return memoryview(m_pointer + first, count);
    }

private:
    const T* m_pointer;
    size_t   m_size;
};

}

#endif // memoryview_hh_INCLUDED

