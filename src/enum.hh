#ifndef enum_hh_INCLUDED
#define enum_hh_INCLUDED

#include "string.hh"

namespace Kakoune
{

template<typename T>
struct EnumDesc
{
    T value;
    StringView name;
};

}

#endif // enum_hh_INCLUDED
