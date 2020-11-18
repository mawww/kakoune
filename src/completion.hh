#ifndef completion_hh_INCLUDED
#define completion_hh_INCLUDED

#include <functional>
#include <algorithm>

#include "units.hh"
#include "string.hh"
#include "vector.hh"
#include "ranked_match.hh"

namespace Kakoune
{

class Context;

using CandidateList = Vector<String, MemoryDomain::Completion>;

struct Completions
{
    enum class Flags
    {
        None = 0,
        Quoted = 0b1,
        Menu = 0b10,
        NoEmpty = 0b100
    };

    constexpr friend bool with_bit_ops(Meta::Type<Flags>) { return true; }

    CandidateList candidates;
    ByteCount start;
    ByteCount end;
    Flags flags = Flags::None;

    Completions()
        : start(0), end(0) {}

    Completions(ByteCount start, ByteCount end)
        : start(start), end(end) {}

    Completions(ByteCount start, ByteCount end, CandidateList candidates, Flags flags = Flags::None)
        : candidates(std::move(candidates)), start(start), end(end), flags{flags} {}
};

enum class CompletionFlags
{
    None = 0,
    Fast = 1 << 0,
    Start = 1 << 2,
};

constexpr bool with_bit_ops(Meta::Type<CompletionFlags>) { return true; }

inline Completions complete_nothing(const Context&, CompletionFlags,
                                    StringView, ByteCount cursor_pos)
{
    return {cursor_pos, cursor_pos};
}

Completions shell_complete(const Context& context, CompletionFlags,
                           StringView, ByteCount cursor_pos);

inline Completions offset_pos(Completions completion, ByteCount offset)
{
    return {completion.start + offset, completion.end + offset,
            std::move(completion.candidates), completion.flags};
}

template<typename Container>
CandidateList complete(StringView query, ByteCount cursor_pos,
                       const Container& container)
{
    using std::begin;
    static_assert(not std::is_same<decltype(*begin(container)), String>::value,
                  "complete require long lived strings, not temporaries");

    query = query.substr(0, cursor_pos);
    Vector<RankedMatch> matches;
    for (const auto& str : container)
    {
        if (RankedMatch match{str, query})
            matches.push_back(match);
    }
    std::sort(matches.begin(), matches.end());
    CandidateList res;
    for (auto& m : matches)
        res.push_back(m.candidate().str());
    return res;
}

}
#endif // completion_hh_INCLUDED
