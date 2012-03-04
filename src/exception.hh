#ifndef exception_hh_INCLUDED
#define exception_hh_INCLUDED

#include <string>

namespace Kakoune
{

struct exception
{
    virtual ~exception() {}
    virtual std::string description() const;
};

struct runtime_error : exception
{
    runtime_error(const std::string& description)
        : m_description(description) {}

    std::string description() const { return m_description; }

private:
    std::string m_description;
};

struct logic_error : exception
{
};

}

#endif // exception_hh_INCLUDED
