#ifndef memory_hh_INCLUDED
#define memory_hh_INCLUDED

#include <cstddef>
#include <new>
#include <utility>

#include "assert.hh"
#include "meta.hh"

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
    Regions,
    Display,
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
    Remote,
    Events,
    Completion,
    Regex,
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
        case MemoryDomain::Regions: return "Regions";
        case MemoryDomain::Display: return "Display";
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
        case MemoryDomain::Remote: return "Remote";
        case MemoryDomain::Events: return "Events";
        case MemoryDomain::Completion: return "Completion";
        case MemoryDomain::Regex: return "Regex";
        case MemoryDomain::Count: break;
    }
    kak_assert(false);
    return "";
}

struct MemoryStats
{
    size_t allocated_bytes;
    size_t allocation_count;
    size_t total_allocation_count;
};

extern MemoryStats memory_stats[(size_t)MemoryDomain::Count];

inline void on_alloc(MemoryDomain domain, size_t size)
{
    auto& stats = memory_stats[(int)domain];
    stats.allocated_bytes += size;
    ++stats.allocation_count;
    ++stats.total_allocation_count;
}

inline void on_dealloc(MemoryDomain domain, size_t size)
{
    auto& stats = memory_stats[(int)domain];
    kak_assert(stats.allocated_bytes >= size);
    stats.allocated_bytes -= size;
    --stats.allocation_count;
}

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
        on_alloc(domain, size);
        return reinterpret_cast<T*>(::operator new(size));
    }

    void deallocate(T* ptr, size_t n)
    {
        size_t size = sizeof(T) * n;
        on_dealloc(domain, size);
        ::operator delete(ptr);
    }
};

template<typename T1, MemoryDomain d1, typename T2, MemoryDomain d2>
constexpr bool operator==(const Allocator<T1, d1>&, const Allocator<T2, d2>&)
{
    return d1 == d2;
}

template<typename T1, MemoryDomain d1, typename T2, MemoryDomain d2>
constexpr bool operator!=(const Allocator<T1, d1>&, const Allocator<T2, d2>&)
{
    return d1 != d2;
}


constexpr MemoryDomain memory_domain(Meta::AnyType) { return MemoryDomain::Undefined; }

template<typename T>
constexpr decltype(T::Domain) memory_domain(Meta::Type<T>) { return T::Domain; }

template<MemoryDomain d>
struct UseMemoryDomain
{
    static constexpr MemoryDomain Domain = d;

    [[gnu::always_inline]]
    static void* operator new(size_t size)
    {
        on_alloc(Domain, size);
        return ::operator new(size);
    }

    [[gnu::always_inline]]
    static void* operator new[](size_t size)
    {
        on_alloc(Domain, size);
        return ::operator new[](size);
    }

    [[gnu::always_inline]]
    static void* operator new(size_t size, void* ptr)
    {
        return ::operator new(size, ptr);
    }

    [[gnu::always_inline]]
    static void operator delete(void* ptr, size_t size)
    {
        on_dealloc(Domain, size);
        ::operator delete(ptr);
    }

    [[gnu::always_inline]]
    static void operator delete[](void* ptr, size_t size)
    {
        on_dealloc(Domain, size);
        ::operator delete[](ptr);
    }
};

}

#endif // memory_hh_INCLUDED
