#ifndef completion_hh_INCLUDED
#define completion_hh_INCLUDED

#include <string>
#include <vector>
#include <functional>

namespace Kakoune
{

typedef std::vector<std::string> CandidateList;

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

CandidateList complete_filename(const std::string& prefix,
                                size_t cursor_pos = std::string::npos);

CandidateList complete_buffername(const std::string& prefix,
                                  size_t cursor_pos = std::string::npos);

typedef std::function<Completions (const std::string&, size_t)> Completer;

struct NullCompletion
{
    Completions operator() (const std::string&, size_t cursor_pos)
    {
        return Completions(cursor_pos, cursor_pos);
    }
};

}
#endif // completion_hh_INCLUDED
