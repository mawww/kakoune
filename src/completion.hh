#ifndef completion_hh_INCLUDED
#define completion_hh_INCLUDED

#include <string>
#include <vector>

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

}
#endif // completion_hh_INCLUDED
