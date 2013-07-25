#ifndef register_hh_INCLUDED
#define register_hh_INCLUDED

#include "memoryview.hh"
#include "string.hh"

namespace Kakoune
{

struct Context;

class Register
{
public:
    virtual ~Register() {}
    virtual Register& operator=(memoryview<String> values) = 0;

    virtual memoryview<String> values(const Context& context) = 0;
};

}

#endif // register_hh_INCLUDED

