#ifndef vector_hh_INCLUDED
#define vector_hh_INCLUDED

#include "memory.hh"
#include "hash.hh"

#include <vector>

namespace Kakoune
{

template<typename T, MemoryDomain domain = memory_domain(Meta::Type<T>{})>
using Vector = std::vector<T, Allocator<T, domain>>;

template<typename T, MemoryDomain domain>
size_t hash_value(const Vector<T, domain>& vector)
{
    size_t hash = 0x1235678;
    for (auto&& elem : vector)
        hash = combine_hash(hash, hash_value(elem));
    return hash;
}

}

#endif // vector_hh_INCLUDED
