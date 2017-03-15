#ifndef enum_hh_INCLUDED
#define enum_hh_INCLUDED

#include "string.hh"

namespace Kakoune
{

template<typename T, size_t N>
struct Array
{
    constexpr size_t size() const { return N; }
    constexpr const T& operator[](int i) const { return m_data[i]; }
    constexpr const T* begin() const { return m_data; }
    constexpr const T* end() const { return m_data+N; }

    T m_data[N];
};

template<typename T> struct EnumDesc { T value; StringView name; };

}

#endif // enum_hh_INCLUDED
