#include "assert.hh"

#include "backtrace.hh"
#include "format.hh"
#include "file.hh"
#include "exception.hh"
#include "debug.hh"

#include <sys/types.h>
#include <unistd.h>

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
    if (not isatty(STDOUT_FILENO) or not isatty(STDIN_FILENO))
        return false;

    write(STDOUT_FILENO, format("\033[;31;5;1mKakoune fatal error, q: exit, i: ignore or debug pid {}\033[0m", getpid()));
    while (true)
    {
        if (unsigned char c = 0; read(STDIN_FILENO, &c, 1) < 0 or c == 'q')
            return false;
        else if (c == 'i')
            return true;
    }
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
