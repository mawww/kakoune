#ifndef memory_hh_INCLUDED
#define memory_hh_INCLUDED

#include <cstdlib>

#include "assert.hh"

namespace Kakoune
{

enum class MemoryDomain
{
    Undefined,
    String,
    InternedString,
    BufferContent,
    BufferMeta,
    WordDB
};

template<MemoryDomain domain>
struct UsedMemory
{
    static size_t byte_count;
};

template<MemoryDomain domain>
size_t UsedMemory<domain>::byte_count = 0;

template<typename T, MemoryDomain domain>
struct Allocator
{
    using value_type = T;

    Allocator() = default;
    template<typename U>
    Allocator(const Allocator<U, domain>&) {}

    template<typename U>
    struct rebind { using other = Allocator<U, domain>; };

    T* allocate(size_t n)
    {
        size_t size = sizeof(T) * n;
        UsedMemory<domain>::byte_count += size;
        return reinterpret_cast<T*>(malloc(size));
    }

    void deallocate(T* ptr, size_t n)
    {
        size_t size = sizeof(T) * n;
        kak_assert(UsedMemory<domain>::byte_count >= size);
        UsedMemory<domain>::byte_count -= size;
        free(ptr);
    }
};

template<typename T1, MemoryDomain d1, typename T2, MemoryDomain d2>
bool operator==(const Allocator<T1, d1>& lhs, const Allocator<T2, d2>& rhs)
{
    return d1 == d2;
}

template<typename T1, MemoryDomain d1, typename T2, MemoryDomain d2>
bool operator!=(const Allocator<T1, d1>& lhs, const Allocator<T2, d2>& rhs)
{
    return d1 != d2;
}

}

#endif // memory_hh_INCLUDED
