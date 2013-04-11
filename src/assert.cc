#include "assert.hh"

#include "exception.hh"

#include <sys/types.h>
#include <unistd.h>

namespace Kakoune
{

struct assert_failed : logic_error
{
    assert_failed(const String& message)
        : m_message(message) {}

    const char* what() const override { return m_message.c_str(); }
private:
    String m_message;
};

void on_assert_failed(const char* message)
{
    String debug_info = "pid: " + int_to_str(getpid());
    int res = system(("xmessage -buttons 'quit:0,ignore:1' '"_str +
                      message + "\n[Debug Infos]\n" + debug_info + "'").c_str());
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
