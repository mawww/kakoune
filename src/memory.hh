#ifndef memory_hh_INCLUDED
#define memory_hh_INCLUDED

#include <cstddef>
#include <cstdlib>
#include <new>
#include <utility>

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
    Options,
    Highlight,
    Mapping,
    Commands,
    Hooks,
    WordDB,
    Count
};

inline const char* domain_name(MemoryDomain domain)
{
    switch (domain)
    {
        case MemoryDomain::Undefined: return "Undefined";
        case MemoryDomain::String: return "String";
        case MemoryDomain::InternedString: return "InternedString";
        case MemoryDomain::BufferContent: return "BufferContent";
        case MemoryDomain::BufferMeta: return "BufferMeta";
        case MemoryDomain::Options: return "Options";
        case MemoryDomain::Highlight: return "Highlight";
        case MemoryDomain::Mapping: return "Mapping";
        case MemoryDomain::Commands: return "Commands";
        case MemoryDomain::Hooks: return "Hooks";
        case MemoryDomain::WordDB: return "WordDB";
        case MemoryDomain::Count: break;
    }
    kak_assert(false);
    return "";
}

extern size_t domain_allocated_bytes[(size_t)MemoryDomain::Count];

template<typename T, MemoryDomain domain>
struct Allocator
{
    using value_type = T;
    // TODO: remove that once we have a c++11 compliant stdlib
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    Allocator() = default;
    template<typename U>
    Allocator(const Allocator<U, domain>&) {}

    template<typename U>
    struct rebind { using other = Allocator<U, domain>; };

    T* allocate(size_t n)
    {
        size_t size = sizeof(T) * n;
        domain_allocated_bytes[(int)domain] += size;
        return reinterpret_cast<T*>(malloc(size));
    }

    void deallocate(T* ptr, size_t n)
    {
        size_t size = sizeof(T) * n;
        kak_assert(domain_allocated_bytes[(int)domain] >= size);
        domain_allocated_bytes[(int)domain] -= size;
        free(ptr);
    }

    template<class U, class... Args>
    void construct(U* p, Args&&... args)
    {
        new ((void*)p) U(std::forward<Args>(args)...);
    }

    template<class U>
    void destroy(U* p) { p->~U(); }
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
