#pragma once

#include "memory.hh"

#include <vector>

namespace Kakoune
{

template<typename T, MemoryDomain domain = memory_domain(Meta::Type<T>{})>
using Vector = std::vector<T, Allocator<T, domain>>;

}
