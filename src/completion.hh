#ifndef completion_hh_INCLUDED
#define completion_hh_INCLUDED

#include "string.hh"

#include <vector>
#include <functional>
#include <unordered_map>

namespace Kakoune
{

class Context;

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

enum class CompletionFlags
{
    None,
    Fast
};
using Completer = std::function<Completions (const Context&, CompletionFlags,
                                             const String&, ByteCount)>;

inline Completions complete_nothing(const Context& context, CompletionFlags,
                                    const String&, ByteCount cursor_pos)
{
    return Completions(cursor_pos, cursor_pos);
}

template<typename Condition, typename Value>
CandidateList complete_key_if(const std::unordered_map<String, Value>& map,
                              const String& prefix,
                              ByteCount cursor_pos,
                              Condition condition)
{
    String real_prefix = prefix.substr(0, cursor_pos);
    CandidateList result;
    for (auto& value : map)
    {
        if (not condition(value))
            continue;

        if (prefix_match(value.first, real_prefix))
            result.push_back(value.first);
    }
    return result;
}

template<typename Value>
CandidateList complete_key(const std::unordered_map<String, Value>& map,
                           const String& prefix,
                           ByteCount cursor_pos)
{
    return complete_key_if(
        map, prefix, cursor_pos,
        [](const std::pair<String, Value>&) { return true; });
}

}
#endif // completion_hh_INCLUDED
