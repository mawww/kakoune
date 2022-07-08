#ifndef array_view_hh_INCLUDED
#define array_view_hh_INCLUDED

#include <initializer_list>
#include <iterator>

namespace Kakoune
{

// An ArrayView provides a typed, non owning view of a memory
// range with an interface similar to std::vector.
template<typename T, typename SizeType = std::size_t>
class ArrayView
{
public:
    using size_t = std::size_t;

    constexpr ArrayView()
        : m_pointer(nullptr), m_size(0) {}

    constexpr ArrayView(T& oneval)
        : m_pointer(&oneval), m_size(1) {}

    constexpr ArrayView(T* pointer, SizeType size)
        : m_pointer(pointer), m_size(size) {}

    constexpr ArrayView(T* begin, T* end)
        : m_pointer(begin), m_size(end - begin) {}

    template<typename It>
        requires std::contiguous_iterator<It> and std::is_same_v<std::iter_value_t<It>, T>
    constexpr ArrayView(It begin, It end)
        : m_pointer(&*begin), m_size(end - begin) {}

    template<size_t N>
    constexpr ArrayView(T(&array)[N]) : m_pointer(array), m_size(N) {}

    template<typename Container>
        requires (sizeof(decltype(*std::declval<Container>().data())) == sizeof(T))
    constexpr ArrayView(Container&& c)
        : m_pointer(c.data()), m_size(c.size()) {}

    constexpr ArrayView(const std::initializer_list<T>& v)
        : m_pointer(v.begin()), m_size(v.size()) {}

    constexpr T* pointer() const { return m_pointer; }
    constexpr SizeType size() const { return m_size; }

    [[gnu::always_inline]]
    constexpr T& operator[](SizeType n) const { return *(m_pointer + (size_t)n); }

    constexpr T* begin() const { return m_pointer; }
    constexpr T* end()   const { return m_pointer+m_size; }

    using reverse_iterator = std::reverse_iterator<T*>;
    constexpr reverse_iterator rbegin() const { return reverse_iterator(m_pointer+m_size); }
    constexpr reverse_iterator rend()   const { return reverse_iterator(m_pointer); }

    constexpr T& front() const { return *m_pointer; }
    constexpr T& back()  const { return *(m_pointer + m_size - 1); }

    constexpr bool empty() const { return m_size == 0; }

    constexpr ArrayView subrange(SizeType first, SizeType count = -1) const
    {
        auto min = [](SizeType a, SizeType b) { return a < b ? a : b; };
        return ArrayView(m_pointer + min(first, m_size),
                         min(count, m_size - min(first, m_size)));
    }

private:
    T* m_pointer;
    SizeType m_size;
};

template<typename It>
    requires std::contiguous_iterator<It>
ArrayView(It begin, It end) -> ArrayView<std::iter_value_t<It>>;

template<typename T, typename SizeType = std::size_t>
using ConstArrayView = ArrayView<const T, SizeType>;


template<typename T, typename SizeType>
bool operator==(ArrayView<T, SizeType> lhs, ArrayView<T, SizeType> rhs)
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

}

#endif // array_view_hh_INCLUDED
