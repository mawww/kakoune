#include "assert.hh"

#include "exception.hh"

namespace Kakoune
{

struct assert_failed : logic_error
{
    assert_failed(const String& message)
        : m_message(message) {}

    String description() const override { return m_message; }
private:
    String m_message;
};

void on_assert_failed(const char* message)
{
    int res = system(("xmessage -buttons 'quit:0,ignore:1' '"_str + message + "'").c_str());
    switch (res)
    {
    case -1:
    case  0:
        throw assert_failed(message);
    case 1:
        return;
    }
}

}
