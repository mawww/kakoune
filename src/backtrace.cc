#include "backtrace.hh"

#include <string.h>
#include <stdlib.h>

#if defined(__linux__) || defined(__APPLE__)
# include <execinfo.h>
#elif defined(__CYGWIN__)
# include <windows.h>
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

char* Backtrace::desc() const
{
    #if defined(__linux__) || defined(__APPLE__)
    char** symbols = backtrace_symbols(stackframes, num_frames);
    int size = 0;
    for (int i = 0; i < num_frames; ++i)
        size += strlen(symbols[i]) + 1;

    char* res = (char*)malloc(size+1);
    res[0] = 0;
    for (int i = 0; i < num_frames; ++i)
    {
        strcat(res, symbols[i]);
        strcat(res, "\n");
    }
    free(symbols);
    return res;
    #elif defined(__CYGWIN__)
    char* res = (char*)malloc(num_frames * 20);
    res[0] = 0;
    for (int i = 0; i < num_frames; ++i)
    {
        char addr[20];
        snprintf(addr, 20, "0x%p", stackframes[i]);
        strcat(res, addr);
    }
    return res;
    #else
    return "<not implemented>";
    #endif
}

}
