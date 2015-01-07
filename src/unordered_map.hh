#ifndef unordered_map_hh_INCLUDED
#define unordered_map_hh_INCLUDED

#include "hash.hh"
#include "memory.hh"

#include <unordered_map>

namespace Kakoune
{

template<typename Key, typename Value, MemoryDomain domain = MemoryDomain::Undefined>
using UnorderedMap = std::unordered_map<Key, Value, Hash<Key>, std::equal_to<Key>,
                                        Allocator<std::pair<const Key, Value>, domain>>;

}

#endif // unordered_map_hh_INCLUDED
