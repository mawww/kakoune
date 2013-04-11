#include "exception.hh"

#include "string.hh"

#include <typeinfo>

namespace Kakoune
{

const char* exception::what() const
{
    return typeid(*this).name();
}

}
