#ifndef completion_hh_INCLUDED
#define completion_hh_INCLUDED

#include <vector>
#include <functional>

#include "string.hh"

namespace Kakoune
{

struct Context;

typedef std::vector<String> CandidateList;

struct Completions
{
    CandidateList candidates;
    ByteCount start;
    ByteCount end;

    Completions()
        : start(0), end(0) {}

    Completions(ByteCount start, ByteCount end)
        : start(start), end(end) {}
};

CandidateList complete_filename(const Context& context,
                                const String& prefix,
                                ByteCount cursor_pos = -1);

typedef std::function<Completions (const Context&,
                                   const String&, ByteCount)> Completer;

inline Completions complete_nothing(const Context& context,
                                    const String&, ByteCount cursor_pos)
{
    return Completions(cursor_pos, cursor_pos);
}

}
#endif // completion_hh_INCLUDED
