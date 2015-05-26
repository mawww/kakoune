#ifndef completion_hh_INCLUDED
#define completion_hh_INCLUDED

#include <functional>

#include "units.hh"
#include "string.hh"
#include "vector.hh"

namespace Kakoune
{

class Context;

using CandidateList = Vector<String>;

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

namespace detail
{
    template<typename Container, typename Func>
    void do_matches(Container&& container, StringView prefix,
                    CandidateList& res, Func match_func)
    {
        for (auto&& elem : container)
            if (match_func(elem, prefix))
                res.push_back(elem);
    }

    template<typename Container, typename Func, typename... Rest>
    void do_matches(Container&& container, StringView prefix,
                    CandidateList& res, Func match_func, Rest... rest)
    {
        do_matches(container, prefix, res, match_func);
        if (res.empty())
            do_matches(container, prefix, res, rest...);
    }
}

template<typename Container, typename... MatchFunc>
CandidateList complete(StringView prefix, ByteCount cursor_pos,
                       const Container& container, MatchFunc... match_func)
{
    CandidateList res;
    detail::do_matches(container, prefix.substr(0, cursor_pos), res, match_func...);
    return res;
}

template<typename Container>
CandidateList complete(StringView prefix, ByteCount cursor_pos,
                       const Container& container)
{
    return complete(prefix, cursor_pos, container, prefix_match);
}

}
#endif // completion_hh_INCLUDED
