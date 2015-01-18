#ifndef memory_hh_INCLUDED
#define memory_hh_INCLUDED

#include <cstddef>
#include <new>
#include <utility>

#include "assert.hh"

namespace Kakoune
{

enum class MemoryDomain
{
    Undefined,
    String,
    SharedString,
    BufferContent,
    BufferMeta,
    Options,
    Highlight,
    Mapping,
    Commands,
    Hooks,
    Aliases,
    EnvVars,
    Faces,
    Values,
    Registers,
    Client,
    WordDB,
    Selections,
    Count
};

inline const char* domain_name(MemoryDomain domain)
{
    switch (domain)
    {
        case MemoryDomain::Undefined: return "Undefined";
        case MemoryDomain::String: return "String";
        case MemoryDomain::SharedString: return "SharedString";
        case MemoryDomain::BufferContent: return "BufferContent";
        case MemoryDomain::BufferMeta: return "BufferMeta";
        case MemoryDomain::Options: return "Options";
        case MemoryDomain::Highlight: return "Highlight";
        case MemoryDomain::Mapping: return "Mapping";
        case MemoryDomain::Commands: return "Commands";
        case MemoryDomain::Hooks: return "Hooks";
        case MemoryDomain::WordDB: return "WordDB";
        case MemoryDomain::Aliases: return "Aliases";
        case MemoryDomain::EnvVars: return "EnvVars";
        case MemoryDomain::Faces: return "Faces";
        case MemoryDomain::Values: return "Values";
        case MemoryDomain::Registers: return "Registers";
        case MemoryDomain::Client: return "Client";
        case MemoryDomain::Selections: return "Selections";
        case MemoryDomain::Count: break;
    }
    kak_assert(false);
    return "";
}

extern size_t domain_allocated_bytes[(size_t)MemoryDomain::Count];

inline void on_alloc(MemoryDomain domain, size_t size)
{
    domain_allocated_bytes[(int)domain] += size;
}

inline void on_dealloc(MemoryDomain domain, size_t size)
{
    kak_assert(domain_allocated_bytes[(int)domain] >= size);
    domain_allocated_bytes[(int)domain] -= size;
}

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
        on_alloc(domain, size);
        return reinterpret_cast<T*>(::operator new(size));
    }

    void deallocate(T* ptr, size_t n)
    {
        size_t size = sizeof(T) * n;
        on_dealloc(domain, size);
        ::operator delete(ptr);
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

template<typename T>
struct TypeDomain
{
    static constexpr MemoryDomain domain = TypeDomain::helper((T*)nullptr);
private:
    template<typename U> static decltype(U::Domain) constexpr helper(U*) { return U::Domain; }
    static constexpr MemoryDomain helper(...) { return MemoryDomain::Undefined; }
};

template<MemoryDomain d>
struct UseMemoryDomain
{
    static constexpr MemoryDomain domain = d;
    static void* operator new(size_t size)
    {
        on_alloc(domain, size);
        return ::operator new(size);
    }

    static void operator delete(void* ptr, size_t size)
    {
        on_dealloc(domain, size);
        ::operator delete(ptr);
    }
};

}

#endif // memory_hh_INCLUDED
