#ifndef register_hh_INCLUDED
#define register_hh_INCLUDED

#include "array_view.hh"
#include "string.hh"

namespace Kakoune
{

class Context;

class Register
{
public:
    virtual ~Register() {}
    virtual Register& operator=(ConstArrayView<String> values) = 0;

    virtual ConstArrayView<String> values(const Context& context) = 0;
};

}

#endif // register_hh_INCLUDED
