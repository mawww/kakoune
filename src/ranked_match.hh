#ifndef ranked_match_hh_INCLUDED
#define ranked_match_hh_INCLUDED

#include "string.hh"
#include "vector.hh"

namespace Kakoune
{

struct RankedMatch
{
    StringView word;
    int rank;
};
using RankedMatchList = Vector<RankedMatch>;

int match_rank(StringView candidate, StringView query);

}

#endif // ranked_match_hh_INCLUDED
