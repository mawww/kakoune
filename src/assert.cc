#include "assert.hh"

namespace Kakoune
{

assert_failed::assert_failed(const String& message)
{
    m_message = message;
}

String assert_failed::description() const
{
    return m_message;
}

}
