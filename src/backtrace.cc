#include "backtrace.hh"

#include "string.hh"

#if defined(__GLIBC__) || defined(__APPLE__)
# include <execinfo.h>
#elif defined(__CYGWIN__)
# include <windows.h>
# include <dbghelp.h>
# include <stdio.h>
#endif

#if defined(__linux__) || defined(__APPLE__)
# include <cstdlib>
#endif

namespace Kakoune
{

Backtrace::Backtrace()
{
    #if defined(__GLIBC__) || defined(__APPLE__)
    num_frames = backtrace(stackframes, max_frames);
    #elif defined(__CYGWIN__)
    num_frames = CaptureStackBackTrace(0, max_frames, stackframes, nullptr);
    #endif
}

String Backtrace::desc() const
{
    #if defined(__GLIBC__) || defined(__APPLE__)
    char** symbols = backtrace_symbols(stackframes, num_frames);
    ByteCount size = 0;
    for (int i = 0; i < num_frames; ++i)
        size += strlen(symbols[i]) + 1;

    String res; res.reserve(size);
    for (int i = 0; i < num_frames; ++i)
    {
        res += symbols[i];
        res += "\n";
    }
    free(symbols);
    return res;
    #elif defined(__CYGWIN__)
    HANDLE process = GetCurrentProcess();

    static bool symbols_initialized = false;
    if (not symbols_initialized)
    {
        SymInitialize(process, nullptr, true);
        symbols_initialized = true;
    }

    alignas(SYMBOL_INFO) char symbol_info_buffer[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO* symbol_info = reinterpret_cast<SYMBOL_INFO*>(symbol_info_buffer);
    symbol_info->MaxNameLen = 255;
    symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);

    String res; // res.reserve(num_frames * 276);
    for (int i = 0; i < num_frames; ++i)
    {
        SymFromAddr(process, (DWORD64)stackframes[i], 0, symbol_info);
        char desc[276];
        snprintf(desc, 276, "0x%0llx (%s)\n", symbol_info->Address, symbol_info->Name);
        res += desc;
    }
    return res;
    #else
    return "<not implemented>";
    #endif
}

}
