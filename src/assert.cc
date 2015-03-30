#include "assert.hh"

#include "exception.hh"
#include "debug.hh"

#if defined(__CYGWIN__)
#include <windows.h>
#endif

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

namespace Kakoune
{

struct assert_failed : logic_error
{
    assert_failed(String message)
        : m_message(std::move(message)) {}

    StringView what() const override { return m_message; }
private:
    String m_message;
};

void on_assert_failed(const char* message)
{
    String debug_info = format("pid: {}", getpid());
    write_debug(format("assert failed: '{}' ", message, debug_info));

    const auto msg = format("{}\n[Debug Infos]\n{}", message, debug_info);
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
    auto cmd = "xmessage -buttons 'quit:0,ignore:1' '" + msg + "'";
    switch (system(cmd.c_str()))
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
