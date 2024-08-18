#ifndef fd_hh_INCLUDED
#define fd_hh_INCLUDED

#include <utility>

namespace Kakoune
{

template<auto close_fn>
struct UniqueDescriptor
{
    UniqueDescriptor(int descriptor = -1) : descriptor{descriptor} {}
    UniqueDescriptor(UniqueDescriptor&& other) : descriptor{other.descriptor} { other.descriptor = -1; }
    UniqueDescriptor& operator=(UniqueDescriptor&& other) { std::swap(descriptor, other.descriptor); other.close(); return *this; }
    ~UniqueDescriptor() { close(); }

    explicit operator int() const { return descriptor; }
    explicit operator bool() const { return descriptor != -1; }
    void close() { if (descriptor != -1) { close_fn(descriptor); descriptor = -1; } }
    int descriptor;
};

}

#endif // fd_hh_INCLUDED
