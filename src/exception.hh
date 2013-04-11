#ifndef exception_hh_INCLUDED
#define exception_hh_INCLUDED

#include "string.hh"

namespace Kakoune
{

struct exception
{
    virtual ~exception() {}
    virtual const char* what() const;
};

struct runtime_error : exception
{
    runtime_error(String what)
        : m_what(std::move(what)) {}

    const char* what() const override { return m_what.c_str(); }

private:
    String m_what;
};

struct logic_error : exception
{
};

}

#endif // exception_hh_INCLUDED
