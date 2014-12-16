#ifndef unordered_map_hh_INCLUDED
#define unordered_map_hh_INCLUDED

#include "hash.hh"

#include <unordered_map>

namespace Kakoune
{

template<typename Key, typename Value>
using UnorderedMap = std::unordered_map<Key, Value, Hash<Key>>;

}

#endif // unordered_map_hh_INCLUDED

