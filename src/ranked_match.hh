#ifndef ranked_match_hh_INCLUDED
#define ranked_match_hh_INCLUDED

#include "string.hh"

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
    bool m_first_char_match = false;
    bool m_prefix = false;
    int m_word_boundary_match_count = 0;
    bool m_only_word_boundary = false;
};

}

#endif // ranked_match_hh_INCLUDED
