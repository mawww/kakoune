#include "exception.hh"

#include "string.hh"

#include <typeinfo>

namespace Kakoune
{

StringView exception::what() const
{
    return typeid(*this).name();
}

}
