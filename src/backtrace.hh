#ifndef backtrace_hh_INCLUDED
#define backtrace_hh_INCLUDED

namespace Kakoune
{

class String;

struct Backtrace
{
    static constexpr int max_frames = 16;
    void* stackframes[max_frames];
    int num_frames = 0;

    Backtrace();
    String desc() const;
};

}

#endif // backtrace_hh_INCLUDED

