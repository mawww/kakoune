#ifndef exception_hh_INCLUDED
#define exception_hh_INCLUDED

#include "string.hh"

namespace Kakoune
{

struct exception
{
    virtual ~exception() {}
    virtual String description() const;
};

struct runtime_error : exception
{
    runtime_error(const String& description)
        : m_description(description) {}

    String description() const { return m_description; }

private:
    String m_description;
};

struct logic_error : exception
{
};

}

#endif // exception_hh_INCLUDED
