#ifndef completion_hh_INCLUDED
#define completion_hh_INCLUDED

#include <vector>
#include <functional>

#include "string.hh"

namespace Kakoune
{

typedef std::vector<String> CandidateList;

struct Completions
{
    CandidateList candidates;
    size_t start;
    size_t end;

    Completions()
        : start(0), end(0) {}

    Completions(size_t start, size_t end)
        : start(start), end(end) {}
};

CandidateList complete_filename(const String& prefix,
                                size_t cursor_pos = -1);

typedef std::function<Completions (const String&, size_t)> Completer;

inline Completions complete_nothing(const String&, size_t cursor_pos)
{
    return Completions(cursor_pos, cursor_pos);
}

}
#endif // completion_hh_INCLUDED
