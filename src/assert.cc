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

void on_assert_failed(const String& message)
{
    int res = system(("xmessage -buttons 'quit:0,ignore:1' '" + message + "'").c_str());
    switch (res)
    {
    case 0:
        throw assert_failed(message);
    case 1:
        return;
    }
}

}
