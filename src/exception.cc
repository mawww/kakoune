#include "exception.hh"

#include "string.hh"

#include <typeinfo>

namespace Kakoune
{

String exception::description() const
{
    return typeid(*this).name();
}

}
