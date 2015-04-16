#ifndef unordered_map_hh_INCLUDED
#define unordered_map_hh_INCLUDED

#include "hash.hh"
#include "memory.hh"

#include <unordered_map>
#include <unordered_set>

namespace Kakoune
{

template<typename Key, typename Value, MemoryDomain domain = TypeDomain<Key>::domain()>
using UnorderedMap = std::unordered_map<Key, Value, Hash<Key>, std::equal_to<Key>,
                                        Allocator<std::pair<const Key, Value>, domain>>;

template<typename Key, MemoryDomain domain = TypeDomain<Key>::domain()>
using UnorderedSet = std::unordered_set<Key, Hash<Key>, std::equal_to<Key>,
                                        Allocator<Key, domain>>;

}

#endif // unordered_map_hh_INCLUDED
