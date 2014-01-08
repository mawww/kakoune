#include "assert.hh"

#include "exception.hh"
#include "debug.hh"

#if defined(__CYGWIN__)
#include <windows.h>
#endif

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
    String debug_info = "pid: " + to_string(getpid());
    write_debug("assert failed: '"_str + message + "' " + debug_info);

    const auto msg = message + "\n[Debug Infos]\n"_str + debug_info;
#if defined(__CYGWIN__)
    int res = MessageBox(NULL, msg.c_str(), "Kakoune: assert failed",
                         MB_OKCANCEL | MB_ICONERROR);
    switch (res)
    {
    case IDCANCEL:
        throw assert_failed(message);
    case IDOK:
        return;
    }
#else
    int res = system(("xmessage -buttons 'quit:0,ignore:1' '" + msg + "'").c_str());
    switch (res)
    {
    case -1:
    case  0:
        throw assert_failed(message);
    case 1:
        return;
    }
#endif
}

}
