#pragma once

#include "string.hh"

namespace Kakoune
{

template<typename T> struct EnumDesc { T value; StringView name; };

}
