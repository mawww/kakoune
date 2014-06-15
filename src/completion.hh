#ifndef completion_hh_INCLUDED
#define completion_hh_INCLUDED

#include <vector>
#include <functional>

#include "units.hh"
#include "string.hh"

namespace Kakoune
{

class Context;

using CandidateList = std::vector<String>;

struct Completions
{
    CandidateList candidates;
    ByteCount start;
    ByteCount end;

    Completions()
        : start(0), end(0) {}

    Completions(ByteCount start, ByteCount end)
        : start(start), end(end) {}

    Completions(ByteCount start, ByteCount end, CandidateList candidates)
        : start(start), end(end), candidates(std::move(candidates)) {}
};

enum class CompletionFlags
{
    None,
    Fast
};
using Completer = std::function<Completions (const Context&, CompletionFlags,
                                             StringView, ByteCount)>;

inline Completions complete_nothing(const Context& context, CompletionFlags,
                                    StringView, ByteCount cursor_pos)
{
    return Completions(cursor_pos, cursor_pos);
}

Completions shell_complete(const Context& context, CompletionFlags,
                           StringView, ByteCount cursor_pos);

inline Completions offset_pos(Completions completion, ByteCount offset)
{
    return { completion.start + offset, completion.end + offset,
             std::move(completion.candidates) };
}

}
#endif // completion_hh_INCLUDED
