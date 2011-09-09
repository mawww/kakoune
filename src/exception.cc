#include "exception.hh"

#include <string>
#include <typeinfo>

namespace Kakoune
{

std::string exception::description() const
{
    return typeid(*this).name();
}

}
