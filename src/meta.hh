#ifndef meta_hh_INCLUDED
#define meta_hh_INCLUDED

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

}

#endif // meta_hh_INCLUDED
