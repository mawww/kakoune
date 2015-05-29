#include "backtrace.hh"

#include "string.hh"

#if defined(__linux__) || defined(__APPLE__)
# include <execinfo.h>
#elif defined(__CYGWIN__)
# include <windows.h>
#include <stdio.h>
#endif

namespace Kakoune
{

Backtrace::Backtrace()
{
    #if defined(__linux__) || defined(__APPLE__)
    num_frames = backtrace(stackframes, max_frames);
    #elif defined(__CYGWIN__)
    num_frames = CaptureStackBackTrace(0, max_frames, stackframes, nullptr);
    #endif
}

String Backtrace::desc() const
{
    #if defined(__linux__) || defined(__APPLE__)
    char** symbols = backtrace_symbols(stackframes, num_frames);
    ByteCount size = 0;
    for (int i = 0; i < num_frames; ++i)
        size += StringView::strlen(symbols[i]) + 1;

    String res; res.reserve(size);
    for (int i = 0; i < num_frames; ++i)
    {
        res += symbols[i];
        res += "\n";
    }
    free(symbols);
    return res;
    #elif defined(__CYGWIN__)
    String res; res.reserve(num_frames * 20);
    for (int i = 0; i < num_frames; ++i)
    {
        char addr[20];
        snprintf(addr, 20, "0x%p", stackframes[i]);
        res += addr;
    }
    return res;
    #else
    return "<not implemented>";
    #endif
}

}
