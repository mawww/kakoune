#include "assert.hh"

namespace Kakoune
{

assert_failed::assert_failed(const std::string& message)
{
    m_message = message;
}

std::string assert_failed::description() const
{
    return m_message;
}

}
