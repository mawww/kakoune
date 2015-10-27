#ifndef ranked_match_hh_INCLUDED
#define ranked_match_hh_INCLUDED

#include "string.hh"
#include "vector.hh"

namespace Kakoune
{

struct RankedMatch
{
    RankedMatch(StringView candidate, StringView query);

    const StringView& candidate() const { return m_candidate; }
    bool operator<(const RankedMatch& other) const;
    bool operator==(const RankedMatch& other) const { return m_candidate == other.m_candidate; }

    explicit operator bool() const { return not m_candidate.empty(); }

private:
    StringView m_candidate;
    int m_match_rank = 0;
};

using RankedMatchList = Vector<RankedMatch>;

}

#endif // ranked_match_hh_INCLUDED
