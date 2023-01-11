#include "assert.hh"

#include "backtrace.hh"
#include "buffer_utils.hh"
#include "exception.hh"

#if defined(__CYGWIN__)
#include <windows.h>
#endif

#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>

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

bool notify_fatal_error(StringView msg)
{
#if defined(__CYGWIN__)
    return MessageBox(NULL, msg.zstr(), "Kakoune: fatal error",
                      MB_OKCANCEL | MB_ICONERROR) == IDOK;
#elif defined(__linux__)
    auto cmd = format("xmessage -buttons 'quit:0,ignore:1' '{}'", replace(msg, "'", "'\\''"));
    int status = system(cmd.c_str());
    return (WIFEXITED(status)) ? (WEXITSTATUS(status)) == 1 : false;
#else
    return false;
#endif
}

void on_assert_failed(const char* message)
{
    String debug_info = format("pid: {}\ncallstack:\n{}", getpid(), Backtrace{}.desc());
    write_to_debug_buffer(format("assert failed: '{}'\n{}", message, debug_info));

    const auto msg = format("{}\n[Debug Infos]\n{}", message, debug_info);
    if (not notify_fatal_error(msg))
        throw assert_failed(msg);
}

}
