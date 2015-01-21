#ifndef vector_hh_INCLUDED
#define vector_hh_INCLUDED

#include "memory.hh"

#include <vector>

namespace Kakoune
{

template<typename T, MemoryDomain domain = TypeDomain<T>::domain()>
using Vector = std::vector<T, Allocator<T, domain>>;

}

#endif // vector_hh_INCLUDED
