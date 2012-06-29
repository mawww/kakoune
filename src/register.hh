#ifndef register_hh_INCLUDED
#define register_hh_INCLUDED

#include "string.hh"
#include "memoryview.hh"

namespace Kakoune
{

class Register
{
public:
    virtual ~Register() {}
    virtual Register& operator=(const memoryview<String>& values) = 0;

    virtual const String& operator[](size_t index) = 0;

    virtual operator memoryview<String>() = 0;
};

}

#endif // register_hh_INCLUDED

