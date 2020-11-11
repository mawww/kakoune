#ifndef enum_hh_INCLUDED
#define enum_hh_INCLUDED

#include "string.hh"
#include "meta.hh"

namespace Kakoune
{

template<typename T> struct EnumDesc { T value; StringView name; };

template<typename T>
concept DescribedEnum = requires { enum_desc(Meta::Type<T>{}); };

}

#endif // enum_hh_INCLUDED
