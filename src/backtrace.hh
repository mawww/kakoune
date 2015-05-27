#ifndef backtrace_hh_INCLUDED
#define backtrace_hh_INCLUDED

namespace Kakoune
{

struct Backtrace
{
    static constexpr int max_frames = 16;
    void* stackframes[max_frames];
    int num_frames = 0;

    Backtrace();
    char* desc() const;
};

}

#endif // backtrace_hh_INCLUDED

