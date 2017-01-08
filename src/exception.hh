#ifndef exception_hh_INCLUDED
#define exception_hh_INCLUDED

#include "string.hh"

namespace Kakoune
{

struct exception
{
    virtual ~exception() = default;
    virtual StringView what() const;
};

struct runtime_error : exception
{
    runtime_error(String what)
        : m_what(std::move(what)) {}

    StringView what() const override { return m_what; }

private:
    String m_what;
};

struct logic_error : exception
{
};

}

#endif // exception_hh_INCLUDED
